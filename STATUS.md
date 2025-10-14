# Berkeley BLISS Fork: Issue Tracking and Development Notes


- **Original Repository:** [n-west/bliss](https://github.com/n-west/bliss)
- **SRT BLISS Testing:** [gbezze/BLISS-for-SRT](https://github.com/gbezze/BLISS-for-SRT/blob/master/bliss_guide_SRT.md)

---

## Current Issues (from Git)

- **_The search runs twice when bliss_find_hits outputs a .dat file_**
- Process dedrift integration + hit search on a per-drift block basis
- bland copy operator requires more sophisticated handling of strides/slices
- bland sum along axis (statistical_launch_wrapper) gives swapped result
- Add option to static link hdf5 and bitshuffle
- Update libfmt to a tagged release with complex support
- improve MAD SNR estimation
- (de)Serialize rfi information for hits
- Add logger


---

## Other Issues / Bugs

- .dat output has the “wrong” whitespace (so find_events doesn’t work)
- There are several branches (relating to issues above) that should be reconciled
- Code/development philosophies are GB-centric
- **_Very high numbers of excess hits at ends of selected drift ranges_**
- Especially when bandpass shape has large change in power (e.g. edge nodes)
- Desmearing algorithm overcompensates for lost power when signals are both wide and high-drift
- Desmearing algorithm uses integer multiples of unit drift rate to estimate lost power (results in “ringing” — could be non-integer?)


---

## Feature Requests

- Can we get this to work with other resolutions (e.g. “mid-res”)?
- Pre filtering (bad rfi zones) so we’re not burying ourselves in false positives
- Search up to 50 Hz/s drift rates.
- Fast enough for the real-time pipelines.
- Integration into the real-time pipelines.


---
