# A parallel recurrent neural network for language modeling
Language modeling using a parallel RNN with POS tags. Paper is here: http://www.aclweb.org/anthology/Y17-1021

This project was developed based on RNNLM Toolkit, http://www.fit.vutbr.cz/~imikolov/rnnlm/
# Compile
Requirements:
>g++

`make`

# Run
See [cmd-swb.sh](cmd-swb.sh)

# Data Format
Please take a look at the [example-data](./example-data) directory. In general, each word is following an integer surrounded by `|`, which corresponds to the word's POS tag.

In the example file name, `40` means `varsize=40` in the paper and `voc-10k` means the vocab size is 10,000. You may change their value according to your needs.
