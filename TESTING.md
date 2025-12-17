# Everything You Need To Know About Testing BLISS

### What Data?

You can find test data here: https://bldata.berkeley.edu/seti_benchmarking

In there are a whole bunch of subdirectories containing data of different sources (real or generated) and from different facilities. More information can, or should, be found in a README.{md,txt} file in each folder. 

This is hardly a complete set of data. From Sofia:

> Things we want in a science data test set:
> * Everyday data from multiple facilities --- check total amount / distribution of hits on real data, make sure no facilities are outliers
> * Data from known "problem" files (e.g., edge nodes) --- check how the different hit finders deal with e.g., low power
> * Data with true positives (Mars, Voyager, setigen) --- see how successfully each method finds known signals
> * Cadences --- mostly for non-unit tests e.g., figuring out SNR for on vs. off in different datasets
> * Different data formats (.fil and .h5, primarily, but potentially also .raw) --- make sure codes run successfully on common formats

Of course we should:
* get as many sample sets as possible to cover all the above cases
* not expect to run all data through BLISS unless there is some major change
* how do categorize which subset of data to run for "minor" changes?

### What Tests?

When testing code changes against the inputs above
1. Does it still compile?
2. Are run times considerably different from previous versions? 
3. Are memory footprints considerably different from previous versions? 
4. Are we seeing expected signals? Unexpected signals?
5. Are known problems still there? For example: the "horseshoe" false positives, or "edge of drift range no matter the drift range" false positives

### Test Policy

We are aiming to streamline/codify/automate the whole test process when making changes to bliss. For example, we are looking into using `pytest`. For now, please do your best at before/after runs of these inputs to check the your changes. 

Difficulties when comparing .dat files, i.e. why can't I just check md5sums of the outputs? 
* BLISS outputs hits in non-deterministic order (even on same system, same file)
* Floating point errors
* How do you compare/code of the qualities of expected signals?
* You should definitely plot the data, but will we miss small but important issues in these plots?
* How fuzzy should the fuzzy comparisons be?
