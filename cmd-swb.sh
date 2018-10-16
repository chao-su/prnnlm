#!/bin/bash

#This is simple example how to use prnnlm tool for training and testing POS language models
#SRILM toolkit must be installed for combination with ngram model to work properly

#make clean
#make

lvrnn=./rnnlm
var=40 # number of classes for the latent variable, integer
hid=100 # hidden size
model_path=./model
lv_train=./example-data/swb.train.lv.${var}.voc-10k
lv_valid=./example-data/swb.valid.lv.${var}.voc-10k
lv_test=./example-data/swb.test.lv.${var}.voc-10k

mkdir ${model_path}

#rnn model is trained here
time $lvrnn -train $lv_train -valid $lv_valid -rnnlm ${model_path}/lvrnn.swb.hid$hid.lv$var -hidden $hid -rand-seed 1 -debug 2 -class 100 -bptt 4 -bptt-block 10 -direct-order 3 -direct 2 -var $var

#ngram model is trained here, using SRILM tools
#ngram-count -text train -order 5 -lm templm -kndiscount -interpolate -gt3min 1 -gt4min 1
#ngram -lm templm -order 5 -ppl test -debug 2 > temp.ppl

#rnn model is tested here
time $lvrnn -rnnlm ${model_path}/lvrnn.swb.hid$hid.lv$var -test $lv_test
