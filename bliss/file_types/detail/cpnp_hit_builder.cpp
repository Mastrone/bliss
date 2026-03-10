#include "cpnp_hit_builder.hpp"
#include <core/hit.hpp>
#include <core/coarse_channel.hpp>

using namespace bliss;
using namespace bliss::detail;

// ============================================================================
// ANONYMOUS NAMESPACE (Helper Functions)
// ============================================================================
namespace {

    /**
     * @brief Extracts Cap'n Proto metadata into a native C++ scan_metadata struct.
     * @param md The Cap'n Proto metadata reader.
     * @return A fully populated scan_metadata struct.
     */
    scan_metadata extract_metadata_from_capnp(const Filterbank::Reader& md) {
        scan_metadata meta;
        meta.fch1        = md.getFch1();
        meta.foff        = md.getFoff();
        meta.machine_id  = 0; // default or missing in capnp
        meta.nbits       = 32; // default or missing in capnp
        meta.nchans      = md.getNumChannels();
        meta.ntsteps     = md.getNumTimesteps();
        meta.nifs        = 0; // default
        meta.source_name = md.getSourceName();
        meta.src_dej     = md.getDec();
        meta.src_raj     = md.getRa();
        meta.telescope_id= md.getTelescopeId();
        meta.tsamp       = md.getTsamp();
        meta.tstart      = md.getTstart();
        meta.data_type   = 1; // default float32
        meta.az_start    = 0; // default
        meta.za_start    = 0; // default
        return meta;
    }

    /**
     * @brief Deserializes a collection of Cap'n Proto signal messages into native C++ hit structs.
     * @param hits_reader The Cap'n Proto list reader containing the signals.
     * @return A standard C++ list of hit structures.
     */
    std::list<hit> extract_hits_from_capnp(const ::capnp::List<Signal, ::capnp::Kind::STRUCT>::Reader& hits_reader) {
        std::list<hit> deserialized_hits;
        auto number_hits = hits_reader.size();
        
        for (size_t hit_index = 0; hit_index < number_hits; ++hit_index) {
            auto this_signal = hits_reader[hit_index];
            hit this_hit = detail::capnp_signal_message_to_bliss_hit(this_signal);
            deserialized_hits.push_back(this_hit);
        }
        return deserialized_hits;
    }
}

// ============================================================================
// SERIALIZATION (C++ to Cap'n Proto)
// ============================================================================

void bliss::detail::bliss_hit_to_capnp_signal_message(Signal::Builder &signal_builder, hit this_hit) {
    signal_builder.setFrequency(this_hit.start_freq_MHz);
    signal_builder.setIndex(this_hit.start_freq_index);
    // redundant with numTimesteps but was a rounded-up power of 2 in seticore
    signal_builder.setDriftSteps(this_hit.time_span_steps);
    signal_builder.setDriftRate(this_hit.drift_rate_Hz_per_sec);
    signal_builder.setDriftIndex(this_hit.rate_index);
    signal_builder.setSnr(this_hit.snr);
    signal_builder.setNumTimesteps(this_hit.time_span_steps); 
    
    // the seticore power was normalized for computing an SNR, the incoherentpower was just the integrated power
    signal_builder.setPower(this_hit.power);
    signal_builder.setIncoherentPower(this_hit.power);
    signal_builder.setStartTime(this_hit.start_time_sec);
    signal_builder.setDurationSeconds(this_hit.duration_sec);

    signal_builder.setBandwidth(this_hit.bandwidth);
    signal_builder.setBinwidth(this_hit.binwidth);
}

void bliss::detail::bliss_coarse_channel_to_capnp_coarse_channel_message(CoarseChannel::Builder &cc_builder, coarse_channel cc) {
    auto md_builder = cc_builder.initMd();
    md_builder.setFch1(cc.fch1());
    md_builder.setDec(cc.src_dej());
    md_builder.setRa(cc.src_raj());
    md_builder.setFoff(cc.foff());
    md_builder.setSourceName(cc.source_name());
    md_builder.setTelescopeId(cc.telescope_id());
    md_builder.setTsamp(cc.tsamp());
    md_builder.setTstart(cc.tstart());
    md_builder.setNumTimesteps(cc.ntsteps());
    md_builder.setNumChannels(cc.nchans());

    auto hits = cc.hits();
    auto hits_builder = cc_builder.initHits(hits.size());
    size_t hit_index = 0;
    
    for (const auto &hit : hits) {
        auto this_signal = hits_builder[hit_index];
        detail::bliss_hit_to_capnp_signal_message(this_signal, hit);
        ++hit_index;
    }
}

void bliss::detail::bliss_scan_to_capnp_scan(Scan::Builder &scan_builder, scan scan_with_hits) {
    auto md_builder = scan_builder.initMd();
    md_builder.setFch1(scan_with_hits.fch1());
    md_builder.setDec(scan_with_hits.src_dej());
    md_builder.setRa(scan_with_hits.src_raj());
    md_builder.setFoff(scan_with_hits.foff());
    md_builder.setSourceName(scan_with_hits.source_name());
    md_builder.setTelescopeId(scan_with_hits.telescope_id());
    md_builder.setTsamp(scan_with_hits.tsamp());
    md_builder.setTstart(scan_with_hits.tstart());
    md_builder.setNumTimesteps(scan_with_hits.ntsteps());
    md_builder.setNumChannels(scan_with_hits.nchans());

    auto number_coarse_channels = scan_with_hits.get_number_coarse_channels();
    auto serialized_coarse_channels = scan_builder.initCoarseChannels(number_coarse_channels);

    for (int cc_index=0; cc_index < number_coarse_channels; ++cc_index) {
        auto cc = scan_with_hits.read_coarse_channel(cc_index);
        if (cc != nullptr) {
            auto cc_builder = serialized_coarse_channels[cc_index];
            bliss_coarse_channel_to_capnp_coarse_channel_message(cc_builder, *cc);
        }
    }
}

void bliss::detail::bliss_observation_target_to_capnp_observation_target_message(ObservationTarget::Builder &obstarget_builder, observation_target observation_with_hits) {
    auto number_scans = observation_with_hits._scans.size();
    auto scan_builders = obstarget_builder.initScans(number_scans);
    
    for (int scan_index=0; scan_index < number_scans; ++scan_index) {
        auto scan_builder = scan_builders[scan_index];
        bliss_scan_to_capnp_scan(scan_builder, observation_with_hits._scans[scan_index]);
    }
}

void bliss::detail::bliss_cadence_to_capnp_cadence_message(Cadence::Builder &cadence_builder, cadence cadence_with_hits) {
    auto number_targets = cadence_with_hits._observations.size();
    auto target_builders = cadence_builder.initObservations(number_targets);
    
    for (int target_index=0; target_index < number_targets; ++target_index) {
        auto obstarget_builder = target_builders[target_index];
        bliss_observation_target_to_capnp_observation_target_message(obstarget_builder, cadence_with_hits._observations[target_index]);
    }
}

// ============================================================================
// DESERIALIZATION (Cap'n Proto to C++)
// ============================================================================

hit bliss::detail::capnp_signal_message_to_bliss_hit(const Signal::Reader &signal_reader) {
    hit this_hit;
    this_hit.start_freq_index      = signal_reader.getIndex();
    this_hit.start_freq_MHz        = signal_reader.getFrequency();
    this_hit.rate_index            = signal_reader.getDriftIndex();
    this_hit.drift_rate_Hz_per_sec = signal_reader.getDriftRate();
    this_hit.power                 = signal_reader.getPower();
    this_hit.time_span_steps       = signal_reader.getNumTimesteps();
    this_hit.snr                   = signal_reader.getSnr();

    this_hit.start_time_sec = signal_reader.getStartTime();
    this_hit.duration_sec   = signal_reader.getDurationSeconds();
    this_hit.bandwidth      = signal_reader.getBandwidth();
    this_hit.binwidth       = signal_reader.getBinwidth();

    return this_hit;
}

coarse_channel bliss::detail::capnp_coarse_channel_message_to_bliss_coarse_channel(CoarseChannel::Reader &coarse_channel_reader) {
    auto md = coarse_channel_reader.getMd();
    auto hits_reader = coarse_channel_reader.getHits();

    scan_metadata meta = extract_metadata_from_capnp(md);
    auto deserialized_cc = coarse_channel(meta);
    deserialized_cc.set_hits(extract_hits_from_capnp(hits_reader));
    
    return deserialized_cc;
}

scan bliss::detail::capnp_scan_message_to_bliss_scan(Scan::Reader &scan_reader) {
    auto capnp_coarse_channels = scan_reader.getCoarseChannels();
    auto md = scan_reader.getMd();

    std::map<int, std::shared_ptr<coarse_channel>> coarse_channels;

    for(int cc_index=0; cc_index < capnp_coarse_channels.size(); ++cc_index) {
        auto cc_reader = capnp_coarse_channels[cc_index];
        auto cc = capnp_coarse_channel_message_to_bliss_coarse_channel(cc_reader);
        coarse_channels.insert({cc_index, std::make_shared<coarse_channel>(cc)});
    }

    scan scan_with_hits(coarse_channels);
    scan_with_hits.set_src_raj(md.getRa());
    scan_with_hits.set_src_dej(md.getDec());
    scan_with_hits.set_source_name(md.getSourceName());
    scan_with_hits.set_tstart(md.getTstart());
    scan_with_hits.set_tsamp(md.getTsamp());
    scan_with_hits.set_fch1(md.getFch1());
    scan_with_hits.set_foff(md.getFoff());

    return scan_with_hits;
}

observation_target bliss::detail::capnp_observation_target_message_to_bliss_observation_target(ObservationTarget::Reader &obstarget_reader) {
    auto capnp_scans = obstarget_reader.getScans();
    observation_target target;

    for (int scan_index=0; scan_index < capnp_scans.size(); ++scan_index) {
        auto capnp_scan = capnp_scans[scan_index];
        target._scans.emplace_back(capnp_scan_message_to_bliss_scan(capnp_scan));
    }
    return target;
}

cadence bliss::detail::capnp_cadence_message_to_cadence(Cadence::Reader &cadence_reader) {
    auto capnp_targets = cadence_reader.getObservations();
    cadence deserialized_cadence;

    for (int scan_index=0; scan_index < capnp_targets.size(); ++scan_index) {
        auto capnp_target = capnp_targets[scan_index];
        deserialized_cadence._observations.emplace_back(capnp_observation_target_message_to_bliss_observation_target(capnp_target));
    }
    return deserialized_cadence;
}