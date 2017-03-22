/*
 * STACK_LSTMCRFMMClassifier.h
 *
 *  Created on: Feb 24, 2016
 *      Author: hongshen chen based on meishan zhang
 */

#ifndef SRC_LSTMCRFMMClassifier_H_
#define SRC_LSTMCRFMMClassifier_H_

#include <iostream>

#include <assert.h>
#include "Example.h"
#include "Feature.h"
#include "Metric.h"
#include "NewLookupTable.h"
#include "N3L.h"

using namespace nr;
using namespace std;
using namespace mshadow;
using namespace mshadow::expr;
using namespace mshadow::utils;

//A native neural network classfier using only word embeddings
template<typename xpu>
class STACK_LSTMCRFMMClassifier {
public:
  STACK_LSTMCRFMMClassifier() {
    _dropOut = 0.5;
  }
  ~STACK_LSTMCRFMMClassifier() {

  }

public:
  LookupTable<xpu> _words;
  LookupTable<xpu> _chars;
  // tag variables
  int _tagNum;
  int _tag_outputSize;
  vector<int> _tagSize;
  vector<int> _tagDim;
  NRVec<LookupTable<xpu> > _tags;

  int _wordcontext, _wordwindow;
  int _wordSize;
  int _wordDim;

  int _charcontext, _charwindow;
  int _charSize;
  int _charDim;
  int _char_outputSize;
  int _char_inputSize;

  int _lstmhiddensize;
  int _hiddensize;
  int _inputsize, _token_representation_size;
  UniLayer<xpu> _olayer_linear;
  UniLayer<xpu> _tanh_project;
  UniLayer<xpu> _tanhchar_project;
  UniLayer<xpu> _baseinput_tanh_project;
  AttentionPooling<xpu> _gatedchar_pooling;
  LSTM<xpu> rnn_left_project;
  LSTM<xpu> rnn_right_project;
  MMCRFLoss<xpu> _crf_layer;

  int _labelSize;

  Metric _eval;

  dtype _dropOut;

  //////////basement model params
  LookupTable<xpu> _in_base_words;
  LookupTable<xpu> _base_chars;
  NewLookupTable<xpu> _base_words;
  // tag variables
  int _base_tagNum;
  int _base_tag_outputSize;
  vector<int> _base_tagSize;
  vector<int> _base_tagDim;
  NRVec<LookupTable<xpu> > _base_tags;

  int _base_wordcontext, _base_wordwindow;
  int _base_wordSize;
  int _base_wordDim;

  int _base_charcontext, _base_charwindow;
  int _base_charSize;
  int _base_charDim;
  int _base_char_outputSize;
  int _base_char_inputSize;

  int _base_lstmhiddensize;
  int _base_hiddensize;
  int _base_inputsize, _base_token_representation_size;
  UniLayer<xpu> _base_olayer_linear;
  UniLayer<xpu> _base_tanh_project;
  UniLayer<xpu> _base_tanhchar_project;
  AttentionPooling<xpu> _base_gatedchar_pooling;
  LSTM<xpu> base_rnn_left_project;
  LSTM<xpu> base_rnn_right_project;
  MMCRFLoss<xpu> _base_crf_layer;

  int _base_labelSize;
  Metric _base_eval;
  dtype _base_dropOut;

public:

  inline void init(int word_extend_size,const NRMat<dtype>& wordEmb, int wordcontext, const NRMat<dtype>& charEmb, int charcontext, const NRVec<NRMat<dtype> >& tagEmbs, int labelSize, int charhiddensize,
      int lstmhiddensize, int hiddensize) {
    _wordcontext = wordcontext;
    _wordwindow = 2 * _wordcontext + 1;
    _wordSize = wordEmb.nrows();
    _wordDim = wordEmb.ncols();
    // tag variables
    _tagNum = tagEmbs.size();
    if (_tagNum > 0) {
      _tagSize.resize(_tagNum);
      _tagDim.resize(_tagNum);
      _tags.resize(_tagNum);
      for (int i = 0; i < _tagNum; i++){
        _tagSize[i] = tagEmbs[i].nrows();
        _tagDim[i] = tagEmbs[i].ncols();
        _tags[i].initial(tagEmbs[i]);
      }
      _tag_outputSize = _tagNum * _tagDim[0];
    }
    else {
      _tag_outputSize = 0;
    }
    _charcontext = charcontext;
    _charwindow = 2 * _charcontext + 1;
    _charSize = charEmb.nrows();
    _charDim = charEmb.ncols();

    _char_inputSize = _charwindow * _charDim;
    _char_outputSize = charhiddensize;

    _labelSize = labelSize;
    _hiddensize = hiddensize;
    _lstmhiddensize = lstmhiddensize;
    _token_representation_size = _wordDim + _char_outputSize + _tag_outputSize+_wordDim;
    _inputsize = _wordwindow * _token_representation_size;

    _words.initial(wordEmb);
    _chars.initial(charEmb);   

    cout << "base words lookuptable size before extend:"<< _in_base_words._nVSize << ":"<<_in_base_words._nDim<< endl;
    _base_words.initial(_in_base_words, word_extend_size);
    _base_wordSize = _base_words._nVSize;
    cout << "base words lookuptable size after  extend:"<< _base_words._nVSize << ":"<<_base_words._nDim << endl;
    cout << "random check:" << endl;
    cout << "old lookuptable:"<<_in_base_words._E[2][3] <<endl;
    cout << "new lookuptable:"<<_base_words._E[2][3] <<endl;
    // _in_base_words.release();

    _baseinput_tanh_project.initial(_wordDim, _base_labelSize, true, 10, 0);
    rnn_left_project.initial(_lstmhiddensize, _inputsize, true, 20);
    rnn_right_project.initial(_lstmhiddensize, _inputsize, false, 30);
    _gatedchar_pooling.initial(_char_outputSize, _wordDim, true, 40);
    _tanhchar_project.initial(_char_outputSize, _char_inputSize, true, 50, 0);
    _tanh_project.initial(_hiddensize, 2 * _lstmhiddensize, true, 55, 0);
    _olayer_linear.initial(_labelSize, _hiddensize, false, 60, 2);

    _crf_layer.initial(_labelSize, 70);

  }

  inline void release() {
    _words.release();
    _chars.release();
    // add tags release
    for (int i = 0; i < _tagNum; i++){
      _tags[i].release();
    }
    _olayer_linear.release();
    _tanh_project.release();
    _tanhchar_project.release();
    _gatedchar_pooling.release();
    rnn_left_project.release();
    rnn_right_project.release();
    _crf_layer.release();

  }

  inline dtype process(const vector<Example>& examples, const vector<Example>& base_examples,int iter) {
    _eval.reset();

    int example_num = examples.size();
    dtype cost = 0.0;
    int offset = 0;
    for (int count = 0; count < example_num; count++) {
      const Example& example = examples[count];
      const Example& base_example = base_examples[count];

      int seq_size = example.m_features.size();

      vector<Tensor<xpu, 2, dtype> > base_input(seq_size), base_inputLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_lstmoutput(seq_size), base_lstmoutputLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_i_project_left(seq_size), base_o_project_left(seq_size), base_f_project_left(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_mc_project_left(seq_size), base_c_project_left(seq_size), base_my_project_left(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_i_project_right(seq_size), base_o_project_right(seq_size), base_f_project_right(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_mc_project_right(seq_size), base_c_project_right(seq_size), base_my_project_right(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_project_left(seq_size), base_project_leftLoss(seq_size), base_project_right(seq_size), base_project_rightLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_project(seq_size), base_projectLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_output(seq_size), base_outputLoss(seq_size);

      vector<Tensor<xpu, 3, dtype> > base_charprime(seq_size), base_charprimeLoss(seq_size), base_charprimeMask(seq_size);
      vector<Tensor<xpu, 3, dtype> > base_charinput(seq_size), base_charinputLoss(seq_size);
      vector<Tensor<xpu, 3, dtype> > base_charhidden(seq_size), base_charhiddenLoss(seq_size);
      vector<Tensor<xpu, 3, dtype> > base_chargatedpoolIndex(seq_size), base_chargateweight(seq_size), base_chargateweightMiddle(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_chargatedpool(seq_size), base_chargatedpoolLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_chargateweightsum(seq_size);
      // tag number
      vector<Tensor<xpu, 3, dtype> > base_tagprime(seq_size), base_tagprimeLoss(seq_size), base_tagprimeMask(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_tagoutput(seq_size), base_tagoutputLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_wordprime(seq_size), base_wordprimeLoss(seq_size), base_wordprimeMask(seq_size);
      vector<Tensor<xpu, 2, dtype> > base_wordrepresent(seq_size), base_wordrepresentLoss(seq_size);

      vector<Tensor<xpu, 2, dtype> > base2input(seq_size), base2inputLoss(seq_size);

      vector<Tensor<xpu, 2, dtype> > input(seq_size), inputLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > lstmoutput(seq_size), lstmoutputLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > i_project_left(seq_size), o_project_left(seq_size), f_project_left(seq_size);
      vector<Tensor<xpu, 2, dtype> > mc_project_left(seq_size), c_project_left(seq_size), my_project_left(seq_size);
      vector<Tensor<xpu, 2, dtype> > i_project_right(seq_size), o_project_right(seq_size), f_project_right(seq_size);
      vector<Tensor<xpu, 2, dtype> > mc_project_right(seq_size), c_project_right(seq_size), my_project_right(seq_size);
      vector<Tensor<xpu, 2, dtype> > project_left(seq_size), project_leftLoss(seq_size), project_right(seq_size), project_rightLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > project(seq_size), projectLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > output(seq_size), outputLoss(seq_size);

      vector<Tensor<xpu, 3, dtype> > charprime(seq_size), charprimeLoss(seq_size), charprimeMask(seq_size);
      vector<Tensor<xpu, 3, dtype> > charinput(seq_size), charinputLoss(seq_size);
      vector<Tensor<xpu, 3, dtype> > charhidden(seq_size), charhiddenLoss(seq_size);
      vector<Tensor<xpu, 3, dtype> > chargatedpoolIndex(seq_size), chargateweight(seq_size), chargateweightMiddle(seq_size);
      vector<Tensor<xpu, 2, dtype> > chargatedpool(seq_size), chargatedpoolLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > chargateweightsum(seq_size);
      // tag number
      vector<Tensor<xpu, 3, dtype> > tagprime(seq_size), tagprimeLoss(seq_size), tagprimeMask(seq_size);
      vector<Tensor<xpu, 2, dtype> > tagoutput(seq_size), tagoutputLoss(seq_size);
      vector<Tensor<xpu, 2, dtype> > wordprime(seq_size), wordprimeLoss(seq_size), wordprimeMask(seq_size);
      vector<Tensor<xpu, 2, dtype> > wordrepresent(seq_size), wordrepresentLoss(seq_size);

      //initialize
      for (int idx = 0; idx < seq_size; idx++) {
      //init base classifier
        const Feature& base_feature = base_example.m_features[idx];

        int base_char_num = base_feature.chars.size();
        base_charprime[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_charDim), d_zero);
        base_charprimeLoss[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_charDim), d_zero);
        base_charprimeMask[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_charDim), d_one);
        base_charinput[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_inputSize), d_zero);
        base_charinputLoss[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_inputSize), d_zero);
        base_charhidden[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
        base_charhiddenLoss[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);

        base_chargatedpoolIndex[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
        base_chargateweightMiddle[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
        base_chargateweight[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
        base_chargateweightsum[idx] = NewTensor<xpu>(Shape2(1, _base_char_outputSize), d_zero);
        base_chargatedpool[idx] = NewTensor<xpu>(Shape2(1, _base_char_outputSize), d_zero);
        base_chargatedpoolLoss[idx] = NewTensor<xpu>(Shape2(1, _base_char_outputSize), d_zero);

        // tag prime init
        if (_base_tagNum > 0) {
          base_tagprime[idx] = NewTensor<xpu>(Shape3(_base_tagNum, 1, _base_tagDim[0]), d_zero);
          base_tagprimeLoss[idx] = NewTensor<xpu>(Shape3(_base_tagNum, 1, _base_tagDim[0]), d_zero);
          base_tagprimeMask[idx] = NewTensor<xpu>(Shape3(_base_tagNum, 1, _base_tagDim[0]), d_one);
          base_tagoutput[idx] = NewTensor<xpu>(Shape2(1, _base_tag_outputSize), d_zero);
          base_tagoutputLoss[idx] = NewTensor<xpu>(Shape2(1, _base_tag_outputSize), d_zero);
        }
        base_wordprime[idx] = NewTensor<xpu>(Shape2(1, _base_wordDim), d_zero);
        base_wordprimeLoss[idx] = NewTensor<xpu>(Shape2(1, _base_wordDim), d_zero);
        base_wordprimeMask[idx] = NewTensor<xpu>(Shape2(1, _base_wordDim), d_one);
        base_wordrepresent[idx] = NewTensor<xpu>(Shape2(1, _base_token_representation_size), d_zero);
        base_wordrepresentLoss[idx] = NewTensor<xpu>(Shape2(1, _base_token_representation_size), d_zero);
        base_input[idx] = NewTensor<xpu>(Shape2(1, _base_inputsize), d_zero);
        base_inputLoss[idx] = NewTensor<xpu>(Shape2(1, _base_inputsize), d_zero);

        base_i_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_o_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_f_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_mc_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_c_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_my_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_project_leftLoss[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);

        base_i_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_o_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_f_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_mc_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_c_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_my_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
        base_project_rightLoss[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);

        base_lstmoutput[idx] = NewTensor<xpu>(Shape2(1, 2 * _base_lstmhiddensize), d_zero);
        base_lstmoutputLoss[idx] = NewTensor<xpu>(Shape2(1, 2 * _base_lstmhiddensize), d_zero);
        base_project[idx] = NewTensor<xpu>(Shape2(1, _base_hiddensize), d_zero);
        base_projectLoss[idx] = NewTensor<xpu>(Shape2(1, _base_hiddensize), d_zero);


        base_output[idx] = NewTensor<xpu>(Shape2(1, _base_labelSize), d_zero);
        base_outputLoss[idx] = NewTensor<xpu>(Shape2(1, _base_labelSize), d_zero);
      //init second classifier
        base2input[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);
        base2inputLoss[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);

        const Feature& feature = example.m_features[idx];

        int char_num = feature.chars.size();
        charprime[idx] = NewTensor<xpu>(Shape3(char_num, 1, _charDim), d_zero);
        charprimeLoss[idx] = NewTensor<xpu>(Shape3(char_num, 1, _charDim), d_zero);
        charprimeMask[idx] = NewTensor<xpu>(Shape3(char_num, 1, _charDim), d_one);
        charinput[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_inputSize), d_zero);
        charinputLoss[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_inputSize), d_zero);
        charhidden[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
        charhiddenLoss[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);

        chargatedpoolIndex[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
        chargateweightMiddle[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
        chargateweight[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
        chargateweightsum[idx] = NewTensor<xpu>(Shape2(1, _char_outputSize), d_zero);
        chargatedpool[idx] = NewTensor<xpu>(Shape2(1, _char_outputSize), d_zero);
        chargatedpoolLoss[idx] = NewTensor<xpu>(Shape2(1, _char_outputSize), d_zero);

        // tag prime init
        if (_tagNum > 0) {
          tagprime[idx] = NewTensor<xpu>(Shape3(_tagNum, 1, _tagDim[0]), d_zero);
          tagprimeLoss[idx] = NewTensor<xpu>(Shape3(_tagNum, 1, _tagDim[0]), d_zero);
          tagprimeMask[idx] = NewTensor<xpu>(Shape3(_tagNum, 1, _tagDim[0]), d_one);
          tagoutput[idx] = NewTensor<xpu>(Shape2(1, _tag_outputSize), d_zero);
          tagoutputLoss[idx] = NewTensor<xpu>(Shape2(1, _tag_outputSize), d_zero);
        }
        wordprime[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);
        wordprimeLoss[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);
        wordprimeMask[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_one);
        wordrepresent[idx] = NewTensor<xpu>(Shape2(1, _token_representation_size), d_zero);
        wordrepresentLoss[idx] = NewTensor<xpu>(Shape2(1, _token_representation_size), d_zero);
        input[idx] = NewTensor<xpu>(Shape2(1, _inputsize), d_zero);
        inputLoss[idx] = NewTensor<xpu>(Shape2(1, _inputsize), d_zero);

        i_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        o_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        f_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        mc_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        c_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        my_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        project_leftLoss[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);

        i_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        o_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        f_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        mc_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        c_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        my_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
        project_rightLoss[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);

        lstmoutput[idx] = NewTensor<xpu>(Shape2(1, 2 * _lstmhiddensize), d_zero);
        lstmoutputLoss[idx] = NewTensor<xpu>(Shape2(1, 2 * _lstmhiddensize), d_zero);        
        project[idx] = NewTensor<xpu>(Shape2(1, _hiddensize), d_zero);
        projectLoss[idx] = NewTensor<xpu>(Shape2(1, _hiddensize), d_zero);


        output[idx] = NewTensor<xpu>(Shape2(1, _labelSize), d_zero);
        outputLoss[idx] = NewTensor<xpu>(Shape2(1, _labelSize), d_zero);
      }

      //forward propagation
      //basement
      for (int idx = 0; idx < seq_size; idx++) {
        const Feature& base_feature = base_example.m_features[idx];
        //linear features should not be dropped out

        srand(iter * example_num + count * seq_size + idx+100);

        const vector<int>& base_words = base_feature.words;
        _base_words.GetEmb(base_words[0], base_wordprime[idx]);

        dropoutcol(base_wordprimeMask[idx], _base_dropOut);
        base_wordprime[idx] = base_wordprime[idx] * base_wordprimeMask[idx];

        const vector<int>& base_chars = base_feature.chars;
        int base_char_num = base_chars.size();

        //charprime
        for (int idy = 0; idy < base_char_num; idy++) {
          _base_chars.GetEmb(base_chars[idy], base_charprime[idx][idy]);
        }

        //char dropout
        for (int idy = 0; idy < base_char_num; idy++) {
          dropoutcol(base_charprimeMask[idx][idy], _base_dropOut);
          base_charprime[idx][idy] = base_charprime[idx][idy] * base_charprimeMask[idx][idy];
        }

        // char context
        windowlized(base_charprime[idx], base_charinput[idx], _base_charcontext);

        // char combination
        _base_tanhchar_project.ComputeForwardScore(base_charinput[idx], base_charhidden[idx]);

        // char gated pooling
        _base_gatedchar_pooling.ComputeForwardScore(base_charhidden[idx], base_wordprime[idx], base_chargateweightMiddle[idx], base_chargateweight[idx], base_chargateweightsum[idx], base_chargatedpoolIndex[idx], base_chargatedpool[idx]);
        // tag prime get
        if (_base_tagNum > 0) {
          const vector<int>& base_tags = base_feature.tags;
          for (int idy = 0; idy < _base_tagNum; idy++) {
            _base_tags[idy].GetEmb(base_tags[idy], base_tagprime[idx][idy]);
          }
          // tag drop out
          for (int idy = 0; idy < _base_tagNum; idy++) {
            dropoutcol(base_tagprimeMask[idx][idy], _base_dropOut);
            base_tagprime[idx][idy] = base_tagprime[idx][idy] * base_tagprimeMask[idx][idy];
          }
          concat(base_tagprime[idx], base_tagoutput[idx]);
        }
      }
      // concat tag input
      if (_base_tagNum > 0) {
        for (int idx = 0; idx < seq_size; idx++) {
          concat(base_wordprime[idx], base_chargatedpool[idx], base_tagoutput[idx], base_wordrepresent[idx]);
        }
      }
      else {
        for (int idx = 0; idx < seq_size; idx++) {
          concat(base_wordprime[idx], base_chargatedpool[idx], base_wordrepresent[idx]);
        }
      }

      windowlized(base_wordrepresent, base_input, _base_wordcontext);

      base_rnn_left_project.ComputeForwardScore(base_input, base_i_project_left, base_o_project_left, base_f_project_left, base_mc_project_left,
          base_c_project_left, base_my_project_left, base_project_left);
      base_rnn_right_project.ComputeForwardScore(base_input, base_i_project_right, base_o_project_right, base_f_project_right, base_mc_project_right,
          base_c_project_right, base_my_project_right, base_project_right);

      for (int idx = 0; idx < seq_size; idx++) {
        concat(base_project_left[idx], base_project_right[idx], base_lstmoutput[idx]);
      }
      _base_tanh_project.ComputeForwardScore(base_lstmoutput, base_project);

      _base_olayer_linear.ComputeForwardScore(base_project, base_output);
      _baseinput_tanh_project.ComputeForwardScore(base_output, base2input);
      //second
      //input setting, and linear setting
      for (int idx = 0; idx < seq_size; idx++) {
        const Feature& feature = example.m_features[idx];
        //linear features should not be dropped out

        srand(iter * example_num + count * seq_size + idx);

        const vector<int>& words = feature.words;
        _words.GetEmb(words[0], wordprime[idx]);

        dropoutcol(wordprimeMask[idx], _dropOut);
        wordprime[idx] = wordprime[idx] * wordprimeMask[idx];

        const vector<int>& chars = feature.chars;
        int char_num = chars.size();

        //charprime
        for (int idy = 0; idy < char_num; idy++) {
          _chars.GetEmb(chars[idy], charprime[idx][idy]);
        }

        //char dropout
        for (int idy = 0; idy < char_num; idy++) {
          dropoutcol(charprimeMask[idx][idy], _dropOut);
          charprime[idx][idy] = charprime[idx][idy] * charprimeMask[idx][idy];
        }

        // char context
        windowlized(charprime[idx], charinput[idx], _charcontext);

        // char combination
        _tanhchar_project.ComputeForwardScore(charinput[idx], charhidden[idx]);

        // char gated pooling
        _gatedchar_pooling.ComputeForwardScore(charhidden[idx], wordprime[idx], chargateweightMiddle[idx], chargateweight[idx], chargateweightsum[idx], chargatedpoolIndex[idx], chargatedpool[idx]);
        // tag prime get 
        if (_tagNum > 0) {
          const vector<int>& tags = feature.tags;
          for (int idy = 0; idy < _tagNum; idy++) {
            _tags[idy].GetEmb(tags[idy], tagprime[idx][idy]);
          }
          // tag drop out
          for (int idy = 0; idy < _tagNum; idy++) {
            dropoutcol(tagprimeMask[idx][idy], _dropOut);
            tagprime[idx][idy] = tagprime[idx][idy] * tagprimeMask[idx][idy];
          }
          concat(tagprime[idx], tagoutput[idx]);
        }
      }

      // concat tag input
      if (_tagNum > 0) {
        for (int idx = 0; idx < seq_size; idx++) {
          concat(wordprime[idx], chargatedpool[idx], tagoutput[idx], base2input[idx], wordrepresent[idx]);
        }
      }
      else {
        for (int idx = 0; idx < seq_size; idx++) {
          concat(wordprime[idx], chargatedpool[idx], base2input[idx], wordrepresent[idx]);
        }        
      }

      windowlized(wordrepresent, input, _wordcontext);

      rnn_left_project.ComputeForwardScore(input, i_project_left, o_project_left, f_project_left, mc_project_left,
          c_project_left, my_project_left, project_left);
      rnn_right_project.ComputeForwardScore(input, i_project_right, o_project_right, f_project_right, mc_project_right,
          c_project_right, my_project_right, project_right);
      for (int idx = 0; idx < seq_size; idx++) {
        concat(project_left[idx], project_right[idx], lstmoutput[idx]);
      }
      _tanh_project.ComputeForwardScore(lstmoutput, project);

      _olayer_linear.ComputeForwardScore(project, output);

      // get delta for each output
      cost += _crf_layer.loss(output, example.m_labels, outputLoss, _eval, example_num);
      // loss backward propagation
      // output
      _olayer_linear.ComputeBackwardLoss(project, output, outputLoss, projectLoss);
      _tanh_project.ComputeBackwardLoss(lstmoutput, project, projectLoss, lstmoutputLoss);
      for (int idx = 0; idx < seq_size; idx++) {
        unconcat(project_leftLoss[idx], project_rightLoss[idx], lstmoutputLoss[idx]);
      }


      // word combination
      rnn_left_project.ComputeBackwardLoss(input, i_project_left, o_project_left, f_project_left, mc_project_left,
          c_project_left, my_project_left, project_left, project_leftLoss, inputLoss);
      rnn_right_project.ComputeBackwardLoss(input, i_project_right, o_project_right, f_project_right, mc_project_right,
          c_project_right, my_project_right, project_right, project_rightLoss, inputLoss);
      // word context
      windowlized_backward(wordrepresentLoss, inputLoss, _wordcontext);
      // decompose loss with tagoutputLoss
      if (_tagNum > 0) {
        for (int idx = 0; idx < seq_size; idx++) {
          unconcat(wordprimeLoss[idx], chargatedpoolLoss[idx], tagoutputLoss[idx], base2inputLoss[idx], wordrepresentLoss[idx]);
          // tag prime loss
          unconcat(tagprimeLoss[idx], tagoutputLoss[idx]);
        }
      }
      else {
        for (int idx = 0; idx < seq_size; idx++) {
          unconcat(wordprimeLoss[idx], chargatedpoolLoss[idx], base2inputLoss[idx], wordrepresentLoss[idx]);
        }
      }
      for (int idx = 0; idx < seq_size; idx++) {
        _gatedchar_pooling.ComputeBackwardLoss(charhidden[idx], wordprime[idx], chargateweightMiddle[idx], chargateweight[idx], chargateweightsum[idx],
            chargatedpoolIndex[idx], chargatedpool[idx], chargatedpoolLoss[idx], charhiddenLoss[idx], wordprimeLoss[idx]);

        //char convolution
        _tanhchar_project.ComputeBackwardLoss(charinput[idx], charhidden[idx], charhiddenLoss[idx], charinputLoss[idx]);

        //char context
        windowlized_backward(charprimeLoss[idx], charinputLoss[idx], _charcontext);
      }
      // word fine tune
      if (_words.bEmbFineTune()) {
        for (int idx = 0; idx < seq_size; idx++) {
          const Feature& feature = example.m_features[idx];
          const vector<int>& words = feature.words;
          wordprimeLoss[idx] = wordprimeLoss[idx] * wordprimeMask[idx];
          _words.EmbLoss(words[0], wordprimeLoss[idx]);
        }
      }
      //tag fine tune
      if (_tagNum > 0) {
        for (int idy = 0; idy < _tagNum; idy++){
          if (_tags[idy].bEmbFineTune()) {
            for (int idx = 0; idx < seq_size; idx++) {
              const Feature& feature = example.m_features[idx];
              const vector<int>& tags = feature.tags;
              tagprimeLoss[idx][idy] = tagprimeLoss[idx][idy] * tagprimeMask[idx][idy];
              _tags[idy].EmbLoss(tags[idy], tagprimeLoss[idx][idy]);
            }
          }
        }
      }
      //char finetune
      if (_chars.bEmbFineTune()) {
        for (int idx = 0; idx < seq_size; idx++) {
          const Feature& feature = example.m_features[idx];
          const vector<int>& chars = feature.chars;
          int char_num = chars.size();
          for (int idy = 0; idy < char_num; idy++) {
            charprimeLoss[idx][idy] = charprimeLoss[idx][idy] * charprimeMask[idx][idy];
            _chars.EmbLoss(chars[idy], charprimeLoss[idx][idy]);
          }
        }
      }
      _baseinput_tanh_project.ComputeBackwardLoss(base_output, base2input, base2inputLoss, base_outputLoss);
      //base backward
      _base_olayer_linear.ComputeBackwardLoss(base_project, base_output, base_outputLoss, base_projectLoss);
      _base_tanh_project.ComputeBackwardLoss(base_lstmoutput, base_project, base_projectLoss, base_lstmoutputLoss);
      for (int idx = 0; idx < seq_size; idx++) {
        unconcat(base_project_leftLoss[idx], base_project_rightLoss[idx], base_lstmoutputLoss[idx]);
      }

      // word combination
      base_rnn_left_project.ComputeBackwardLoss(base_input, base_i_project_left, base_o_project_left, base_f_project_left, base_mc_project_left,
          base_c_project_left, base_my_project_left, base_project_left, base_project_leftLoss, base_inputLoss);
      base_rnn_right_project.ComputeBackwardLoss(base_input, base_i_project_right, base_o_project_right, base_f_project_right, base_mc_project_right,
          base_c_project_right, base_my_project_right, base_project_right, base_project_rightLoss, base_inputLoss);
      // word context
      windowlized_backward(base_wordrepresentLoss, base_inputLoss, _base_wordcontext);
      // decompose loss with tagoutputLoss
      if (_base_tagNum > 0) {
        for (int idx = 0; idx < seq_size; idx++) {
          unconcat(base_wordprimeLoss[idx], base_chargatedpoolLoss[idx], base_tagoutputLoss[idx], wordrepresentLoss[idx]);
          // tag prime loss
          unconcat(base_tagprimeLoss[idx], base_tagoutputLoss[idx]);
        }
      }
      else {
        for (int idx = 0; idx < seq_size; idx++) {
          unconcat(base_wordprimeLoss[idx], base_chargatedpoolLoss[idx], base_wordrepresentLoss[idx]);
        }
      }
      for (int idx = 0; idx < seq_size; idx++) {
        _base_gatedchar_pooling.ComputeBackwardLoss(base_charhidden[idx], base_wordprime[idx], base_chargateweightMiddle[idx], base_chargateweight[idx], base_chargateweightsum[idx],
            base_chargatedpoolIndex[idx], base_chargatedpool[idx], base_chargatedpoolLoss[idx], base_charhiddenLoss[idx], base_wordprimeLoss[idx]);

        //char convolution
        _base_tanhchar_project.ComputeBackwardLoss(base_charinput[idx], base_charhidden[idx], base_charhiddenLoss[idx], base_charinputLoss[idx]);

        //char context
        windowlized_backward(base_charprimeLoss[idx], base_charinputLoss[idx], _base_charcontext);
      }
      // word fine tune
      if (_base_words.bEmbFineTune()) {
        for (int idx = 0; idx < seq_size; idx++) {
          const Feature& base_feature = base_example.m_features[idx];
          const vector<int>& base_words = base_feature.words;
          base_wordprimeLoss[idx] = base_wordprimeLoss[idx] * base_wordprimeMask[idx];
          _base_words.EmbLoss(base_words[0], base_wordprimeLoss[idx]);
        }
      }
      //tag fine tune
      if (_base_tagNum > 0) {
        for (int idy = 0; idy < _base_tagNum; idy++){
          if (_base_tags[idy].bEmbFineTune()) {
            for (int idx = 0; idx < seq_size; idx++) {
              const Feature& base_feature = base_example.m_features[idx];
              const vector<int>& base_tags = base_feature.tags;
              base_tagprimeLoss[idx][idy] = base_tagprimeLoss[idx][idy] * base_tagprimeMask[idx][idy];
              _base_tags[idy].EmbLoss(base_tags[idy], base_tagprimeLoss[idx][idy]);
            }
          }
        }
      }
      //char finetune
      if (_base_chars.bEmbFineTune()) {
        for (int idx = 0; idx < seq_size; idx++) {
          const Feature& base_feature = base_example.m_features[idx];
          const vector<int>& base_chars = base_feature.chars;
          int base_char_num = base_chars.size();
          for (int idy = 0; idy < base_char_num; idy++) {
            base_charprimeLoss[idx][idy] = base_charprimeLoss[idx][idy] * base_charprimeMask[idx][idy];
            _base_chars.EmbLoss(base_chars[idy], base_charprimeLoss[idx][idy]);
          }
        }
      }
      //release
      for (int idx = 0; idx < seq_size; idx++) {
        FreeSpace(&(charprime[idx]));
        FreeSpace(&(charprimeLoss[idx]));
        FreeSpace(&(charprimeMask[idx]));
        FreeSpace(&(charinput[idx]));
        FreeSpace(&(charinputLoss[idx]));
        FreeSpace(&(charhidden[idx]));
        FreeSpace(&(charhiddenLoss[idx]));
        FreeSpace(&(chargatedpoolIndex[idx]));
        FreeSpace(&(chargateweightMiddle[idx]));
        FreeSpace(&(chargateweight[idx]));
        FreeSpace(&(chargateweightsum[idx]));
        FreeSpace(&(chargatedpool[idx]));
        FreeSpace(&(chargatedpoolLoss[idx]));

        // tag freespace
        if (_tagNum > 0) {
          FreeSpace(&(tagprime[idx]));
          FreeSpace(&(tagprimeLoss[idx]));
          FreeSpace(&(tagprimeMask[idx]));
          FreeSpace(&(tagoutput[idx]));
          FreeSpace(&(tagoutputLoss[idx]));
        }
        FreeSpace(&(wordprime[idx]));
        FreeSpace(&(wordprimeLoss[idx]));
        FreeSpace(&(wordprimeMask[idx]));
        FreeSpace(&(wordrepresent[idx]));
        FreeSpace(&(wordrepresentLoss[idx]));

        FreeSpace(&(input[idx]));
        FreeSpace(&(inputLoss[idx]));

        FreeSpace(&(i_project_left[idx]));
        FreeSpace(&(o_project_left[idx]));
        FreeSpace(&(f_project_left[idx]));
        FreeSpace(&(mc_project_left[idx]));
        FreeSpace(&(c_project_left[idx]));
        FreeSpace(&(my_project_left[idx]));
        FreeSpace(&(project_left[idx]));
        FreeSpace(&(project_leftLoss[idx]));

        FreeSpace(&(i_project_right[idx]));
        FreeSpace(&(o_project_right[idx]));
        FreeSpace(&(f_project_right[idx]));
        FreeSpace(&(mc_project_right[idx]));
        FreeSpace(&(c_project_right[idx]));
        FreeSpace(&(my_project_right[idx]));
        FreeSpace(&(project_right[idx]));
        FreeSpace(&(project_rightLoss[idx]));

        FreeSpace(&(lstmoutput[idx]));
        FreeSpace(&(project[idx]));
        FreeSpace(&(projectLoss[idx]));
        FreeSpace(&(lstmoutputLoss[idx]));
        FreeSpace(&(output[idx]));
        FreeSpace(&(outputLoss[idx]));

        FreeSpace(&(base2input[idx]));
        FreeSpace(&(base2inputLoss[idx]));
        //release basement
        FreeSpace(&(base_charprime[idx]));
        FreeSpace(&(base_charprimeLoss[idx]));
        FreeSpace(&(base_charprimeMask[idx]));
        FreeSpace(&(base_charinput[idx]));
        FreeSpace(&(base_charinputLoss[idx]));
        FreeSpace(&(base_charhidden[idx]));
        FreeSpace(&(base_charhiddenLoss[idx]));
        FreeSpace(&(base_chargatedpoolIndex[idx]));
        FreeSpace(&(base_chargateweightMiddle[idx]));
        FreeSpace(&(base_chargateweight[idx]));
        FreeSpace(&(base_chargateweightsum[idx]));
        FreeSpace(&(base_chargatedpool[idx]));
        FreeSpace(&(base_chargatedpoolLoss[idx]));

        // tag freespace
        if (_base_tagNum > 0) {
          FreeSpace(&(base_tagprime[idx]));
          FreeSpace(&(base_tagprimeLoss[idx]));
          FreeSpace(&(base_tagprimeMask[idx]));
          FreeSpace(&(base_tagoutput[idx]));
          FreeSpace(&(base_tagoutputLoss[idx]));
        }
        FreeSpace(&(base_wordprime[idx]));
        FreeSpace(&(base_wordprimeLoss[idx]));
        FreeSpace(&(base_wordprimeMask[idx]));
        FreeSpace(&(base_wordrepresent[idx]));
        FreeSpace(&(base_wordrepresentLoss[idx]));

        FreeSpace(&(base_input[idx]));
        FreeSpace(&(base_inputLoss[idx]));

        FreeSpace(&(base_i_project_left[idx]));
        FreeSpace(&(base_o_project_left[idx]));
        FreeSpace(&(base_f_project_left[idx]));
        FreeSpace(&(base_mc_project_left[idx]));
        FreeSpace(&(base_c_project_left[idx]));
        FreeSpace(&(base_my_project_left[idx]));
        FreeSpace(&(base_project_left[idx]));
        FreeSpace(&(base_project_leftLoss[idx]));

        FreeSpace(&(base_i_project_right[idx]));
        FreeSpace(&(base_o_project_right[idx]));
        FreeSpace(&(base_f_project_right[idx]));
        FreeSpace(&(base_mc_project_right[idx]));
        FreeSpace(&(base_c_project_right[idx]));
        FreeSpace(&(base_my_project_right[idx]));
        FreeSpace(&(base_project_right[idx]));
        FreeSpace(&(base_project_rightLoss[idx]));

        FreeSpace(&(base_lstmoutput[idx]));
        FreeSpace(&(base_project[idx]));
        FreeSpace(&(base_projectLoss[idx]));
        FreeSpace(&(base_lstmoutputLoss[idx]));
        FreeSpace(&(base_output[idx]));
        FreeSpace(&(base_outputLoss[idx]));
      }
    }

    if (_eval.getAccuracy() < 0) {
      std::cout << "strange" << std::endl;
    }

    return cost;
  }

  void predict(const vector<Feature>& features, const vector<Feature>& base_features,vector<int>& results) {
    int seq_size = features.size();
    int offset = 0;
    //basement classifier
    vector<Tensor<xpu, 2, dtype> > base_input(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_lstmoutput(seq_size);

    vector<Tensor<xpu, 2, dtype> > base_i_project_left(seq_size), base_o_project_left(seq_size), base_f_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_mc_project_left(seq_size), base_c_project_left(seq_size), base_my_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_i_project_right(seq_size), base_o_project_right(seq_size), base_f_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_mc_project_right(seq_size), base_c_project_right(seq_size), base_my_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_project_left(seq_size), base_project_right(seq_size);

    vector<Tensor<xpu, 2, dtype> > base_project(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_output(seq_size);

    vector<Tensor<xpu, 3, dtype> > base_charprime(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_charinput(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_charhidden(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_chargatedpoolIndex(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_chargatedpool(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_chargateweightMiddle(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_chargateweight(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_chargateweightsum(seq_size);
    // tag number
    vector<Tensor<xpu, 3, dtype> > base_tagprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_tagoutput(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_wordprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_wordrepresent(seq_size);

    vector<Tensor<xpu, 2, dtype> > base2input(seq_size);

    //second classifier
    vector<Tensor<xpu, 2, dtype> > input(seq_size);
    vector<Tensor<xpu, 2, dtype> > lstmoutput(seq_size);

    vector<Tensor<xpu, 2, dtype> > i_project_left(seq_size), o_project_left(seq_size), f_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > mc_project_left(seq_size), c_project_left(seq_size), my_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > i_project_right(seq_size), o_project_right(seq_size), f_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > mc_project_right(seq_size), c_project_right(seq_size), my_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > project_left(seq_size), project_right(seq_size);

    vector<Tensor<xpu, 2, dtype> > project(seq_size);
    vector<Tensor<xpu, 2, dtype> > output(seq_size);

    vector<Tensor<xpu, 3, dtype> > charprime(seq_size);
    vector<Tensor<xpu, 3, dtype> > charinput(seq_size);
    vector<Tensor<xpu, 3, dtype> > charhidden(seq_size);
    vector<Tensor<xpu, 3, dtype> > chargatedpoolIndex(seq_size);
    vector<Tensor<xpu, 2, dtype> > chargatedpool(seq_size);
    vector<Tensor<xpu, 3, dtype> > chargateweightMiddle(seq_size);
    vector<Tensor<xpu, 3, dtype> > chargateweight(seq_size);
    vector<Tensor<xpu, 2, dtype> > chargateweightsum(seq_size);
    vector<Tensor<xpu, 3, dtype> > tagprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > tagoutput(seq_size);
    vector<Tensor<xpu, 2, dtype> > wordprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > wordrepresent(seq_size);

    //initialize
    for (int idx = 0; idx < seq_size; idx++) {
      //basement init
      const Feature& base_feature = base_features[idx];

      int base_char_num = base_feature.chars.size();
      base_charprime[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_charDim), d_zero);
      base_charinput[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_inputSize), d_zero);
      base_charhidden[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargatedpoolIndex[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargatedpool[idx] = NewTensor<xpu>(Shape2(1, _base_char_outputSize), d_zero);
      base_chargateweightMiddle[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargateweight[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargateweightsum[idx] = NewTensor<xpu>(Shape2(1, _base_char_outputSize), d_zero);
      if (_base_tagNum > 0) {
        base_tagprime[idx] = NewTensor<xpu>(Shape3(_base_tagNum, 1, _base_tagDim[0]), d_zero);
        base_tagoutput[idx] = NewTensor<xpu>(Shape2(1, _base_tag_outputSize), d_zero);
      }
      base_wordprime[idx] = NewTensor<xpu>(Shape2(1, _base_wordDim), d_zero);
      base_wordrepresent[idx] = NewTensor<xpu>(Shape2(1, _base_token_representation_size), d_zero);
      base_input[idx] = NewTensor<xpu>(Shape2(1, _base_inputsize), d_zero);

      base_i_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_o_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_f_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_mc_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_c_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_my_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);

      base_i_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_o_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_f_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_mc_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_c_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_my_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);

      base_lstmoutput[idx] = NewTensor<xpu>(Shape2(1, 2 * _base_lstmhiddensize), d_zero);
      base_project[idx] = NewTensor<xpu>(Shape2(1, _base_hiddensize), d_zero);
      base_output[idx] = NewTensor<xpu>(Shape2(1, _base_labelSize), d_zero);

      base2input[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);
      //secondary init
      const Feature& feature = features[idx];

      int char_num = feature.chars.size();
      charprime[idx] = NewTensor<xpu>(Shape3(char_num, 1, _charDim), d_zero);
      charinput[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_inputSize), d_zero);
      charhidden[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargatedpoolIndex[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargatedpool[idx] = NewTensor<xpu>(Shape2(1, _char_outputSize), d_zero);
      chargateweightMiddle[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargateweight[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargateweightsum[idx] = NewTensor<xpu>(Shape2(1, _char_outputSize), d_zero);
      if (_tagNum > 0) {
        tagprime[idx] = NewTensor<xpu>(Shape3(_tagNum, 1, _tagDim[0]), d_zero);
        tagoutput[idx] = NewTensor<xpu>(Shape2(1, _tag_outputSize), d_zero);
      }
      wordprime[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);
      wordrepresent[idx] = NewTensor<xpu>(Shape2(1, _token_representation_size), d_zero);
      input[idx] = NewTensor<xpu>(Shape2(1, _inputsize), d_zero);

      i_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      o_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      f_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      mc_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      c_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      my_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);

      i_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      o_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      f_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      mc_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      c_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      my_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);

      lstmoutput[idx] = NewTensor<xpu>(Shape2(1, 2 * _lstmhiddensize), d_zero);
      project[idx] = NewTensor<xpu>(Shape2(1, _hiddensize), d_zero);
      output[idx] = NewTensor<xpu>(Shape2(1, _labelSize), d_zero);
    }

    //forward propagation
    //basement input setting, and linear setting
    //input setting, and linear setting
    for (int idx = 0; idx < seq_size; idx++) {
      const Feature& base_feature = base_features[idx];
      //linear features should not be dropped out

      const vector<int>& base_words = base_feature.words;
      _base_words.GetEmb(base_words[0], base_wordprime[idx]);

      const vector<int>& base_chars = base_feature.chars;
      int base_char_num = base_chars.size();

      //charprime
      for (int idy = 0; idy < base_char_num; idy++) {
        _base_chars.GetEmb(base_chars[idy], base_charprime[idx][idy]);
      }

      // char context
      windowlized(base_charprime[idx], base_charinput[idx], _base_charcontext);

      // char combination
      _base_tanhchar_project.ComputeForwardScore(base_charinput[idx], base_charhidden[idx]);

      // char gated pooling
      _base_gatedchar_pooling.ComputeForwardScore(base_charhidden[idx], base_wordprime[idx], base_chargateweightMiddle[idx], base_chargateweight[idx], base_chargateweightsum[idx], base_chargatedpoolIndex[idx], base_chargatedpool[idx]);
      // tag prime get
      if (_base_tagNum > 0) {
        const vector<int>& base_tags = base_feature.tags;
        for (int idy = 0; idy < _base_tagNum; idy++){
          _base_tags[idy].GetEmb(base_tags[idy], base_tagprime[idx][idy]);
        }
        concat(base_tagprime[idx], base_tagoutput[idx]);
      }
    }
    if (_base_tagNum > 0) {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(base_wordprime[idx], base_chargatedpool[idx], base_tagoutput[idx], base_wordrepresent[idx]);
      }
    }
    else {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(base_wordprime[idx], base_chargatedpool[idx], base_wordrepresent[idx]);
      }
    }

    windowlized(base_wordrepresent, base_input, _base_wordcontext);

    base_rnn_left_project.ComputeForwardScore(base_input, base_i_project_left, base_o_project_left, base_f_project_left, base_mc_project_left,
        base_c_project_left, base_my_project_left, base_project_left);
    base_rnn_right_project.ComputeForwardScore(base_input, base_i_project_right, base_o_project_right, base_f_project_right, base_mc_project_right,
        base_c_project_right, base_my_project_right, base_project_right);

    for (int idx = 0; idx < seq_size; idx++) {
      concat(base_project_left[idx], base_project_right[idx], base_lstmoutput[idx]);
    }
    _base_tanh_project.ComputeForwardScore(base_lstmoutput, base_project);

    _base_olayer_linear.ComputeForwardScore(base_project, base_output);

    _baseinput_tanh_project.ComputeForwardScore(base_output, base2input);
    //secondary input setting, and linear setting
    for (int idx = 0; idx < seq_size; idx++) {
      const Feature& feature = features[idx];
      //linear features should not be dropped out

      const vector<int>& words = feature.words;
      _words.GetEmb(words[0], wordprime[idx]);

      const vector<int>& chars = feature.chars;
      int char_num = chars.size();

      //charprime
      for (int idy = 0; idy < char_num; idy++) {
        _chars.GetEmb(chars[idy], charprime[idx][idy]);
      }

      // char context
      windowlized(charprime[idx], charinput[idx], _charcontext);

      // char combination
      _tanhchar_project.ComputeForwardScore(charinput[idx], charhidden[idx]);

      // char gated pooling
      _gatedchar_pooling.ComputeForwardScore(charhidden[idx], wordprime[idx], chargateweightMiddle[idx], chargateweight[idx], chargateweightsum[idx], chargatedpoolIndex[idx], chargatedpool[idx]);
      // tag prime get
      if (_tagNum > 0) {
        const vector<int>& tags = feature.tags;
        for (int idy = 0; idy < _tagNum; idy++){
          _tags[idy].GetEmb(tags[idy], tagprime[idx][idy]);
        }
        concat(tagprime[idx], tagoutput[idx]); 
      } 
    }
    if (_tagNum > 0) {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(wordprime[idx], chargatedpool[idx], tagoutput[idx], base2input[idx], wordrepresent[idx]);
      }
    }
    else {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(wordprime[idx], chargatedpool[idx], base2input[idx], wordrepresent[idx]);
      }      
    }

    windowlized(wordrepresent, input, _wordcontext);

    rnn_left_project.ComputeForwardScore(input, i_project_left, o_project_left, f_project_left, mc_project_left,
        c_project_left, my_project_left, project_left);
    rnn_right_project.ComputeForwardScore(input, i_project_right, o_project_right, f_project_right, mc_project_right,
        c_project_right, my_project_right, project_right);

    for (int idx = 0; idx < seq_size; idx++) {
      concat(project_left[idx], project_right[idx], lstmoutput[idx]);
    }
    _tanh_project.ComputeForwardScore(lstmoutput, project);

    _olayer_linear.ComputeForwardScore(project, output);

    // decode algorithm
    _crf_layer.predict(output, results);

    //release
    for (int idx = 0; idx < seq_size; idx++) {
      FreeSpace(&(charprime[idx]));
      FreeSpace(&(charinput[idx]));
      FreeSpace(&(charhidden[idx]));
      FreeSpace(&(chargatedpoolIndex[idx]));
      FreeSpace(&(chargatedpool[idx]));
      FreeSpace(&(chargateweightMiddle[idx]));
      FreeSpace(&(chargateweight[idx]));
      FreeSpace(&(chargateweightsum[idx]));
      if (_tagNum > 0) {
        FreeSpace(&(tagprime[idx]));
        FreeSpace(&(tagoutput[idx]));
      }
      FreeSpace(&(wordprime[idx]));
      FreeSpace(&(wordrepresent[idx]));
      FreeSpace(&(input[idx]));

      FreeSpace(&(i_project_left[idx]));
      FreeSpace(&(o_project_left[idx]));
      FreeSpace(&(f_project_left[idx]));
      FreeSpace(&(mc_project_left[idx]));
      FreeSpace(&(c_project_left[idx]));
      FreeSpace(&(my_project_left[idx]));
      FreeSpace(&(project_left[idx]));

      FreeSpace(&(i_project_right[idx]));
      FreeSpace(&(o_project_right[idx]));
      FreeSpace(&(f_project_right[idx]));
      FreeSpace(&(mc_project_right[idx]));
      FreeSpace(&(c_project_right[idx]));
      FreeSpace(&(my_project_right[idx]));
      FreeSpace(&(project_right[idx]));

      FreeSpace(&(lstmoutput[idx]));
      FreeSpace(&(project[idx]));
      FreeSpace(&(output[idx]));
      FreeSpace(&(base2input[idx]));
      //release basement
      FreeSpace(&(base_charprime[idx]));
      FreeSpace(&(base_charinput[idx]));
      FreeSpace(&(base_charhidden[idx]));
      FreeSpace(&(base_chargatedpoolIndex[idx]));
      FreeSpace(&(base_chargatedpool[idx]));
      FreeSpace(&(base_chargateweightMiddle[idx]));
      FreeSpace(&(base_chargateweight[idx]));
      FreeSpace(&(base_chargateweightsum[idx]));
      if (_base_tagNum > 0) {
        FreeSpace(&(base_tagprime[idx]));
        FreeSpace(&(base_tagoutput[idx]));
      }
      FreeSpace(&(base_wordprime[idx]));
      FreeSpace(&(base_wordrepresent[idx]));
      FreeSpace(&(base_input[idx]));

      FreeSpace(&(base_i_project_left[idx]));
      FreeSpace(&(base_o_project_left[idx]));
      FreeSpace(&(base_f_project_left[idx]));
      FreeSpace(&(base_mc_project_left[idx]));
      FreeSpace(&(base_c_project_left[idx]));
      FreeSpace(&(base_my_project_left[idx]));
      FreeSpace(&(base_project_left[idx]));

      FreeSpace(&(base_i_project_right[idx]));
      FreeSpace(&(base_o_project_right[idx]));
      FreeSpace(&(base_f_project_right[idx]));
      FreeSpace(&(base_mc_project_right[idx]));
      FreeSpace(&(base_c_project_right[idx]));
      FreeSpace(&(base_my_project_right[idx]));
      FreeSpace(&(base_project_right[idx]));

      FreeSpace(&(base_lstmoutput[idx]));
      FreeSpace(&(base_project[idx]));
      FreeSpace(&(base_output[idx]));
    }
  }

  dtype computeScore(const Example& example, const Example& base_example) {
    int seq_size = example.m_features.size();
    int offset = 0;
    //basement params
    vector<Tensor<xpu, 2, dtype> > base_input(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_lstmoutput(seq_size);

    vector<Tensor<xpu, 2, dtype> > base_i_project_left(seq_size), base_o_project_left(seq_size), base_f_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_mc_project_left(seq_size), base_c_project_left(seq_size), base_my_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_i_project_right(seq_size), base_o_project_right(seq_size), base_f_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_mc_project_right(seq_size), base_c_project_right(seq_size), base_my_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_project_left(seq_size), base_project_right(seq_size);

    vector<Tensor<xpu, 2, dtype> > base_project(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_output(seq_size);

    vector<Tensor<xpu, 3, dtype> > base_charprime(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_charinput(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_charhidden(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_chargatedpoolIndex(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_chargatedpool(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_chargateweightMiddle(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_chargateweight(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_chargateweightsum(seq_size);
    vector<Tensor<xpu, 3, dtype> > base_tagprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_tagoutput(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_wordprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > base_wordrepresent(seq_size);

    vector<Tensor<xpu, 2, dtype> > base2input(seq_size);

    //secondary params
    vector<Tensor<xpu, 2, dtype> > input(seq_size);
    vector<Tensor<xpu, 2, dtype> > lstmoutput(seq_size);

    vector<Tensor<xpu, 2, dtype> > i_project_left(seq_size), o_project_left(seq_size), f_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > mc_project_left(seq_size), c_project_left(seq_size), my_project_left(seq_size);
    vector<Tensor<xpu, 2, dtype> > i_project_right(seq_size), o_project_right(seq_size), f_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > mc_project_right(seq_size), c_project_right(seq_size), my_project_right(seq_size);
    vector<Tensor<xpu, 2, dtype> > project_left(seq_size), project_right(seq_size);

    vector<Tensor<xpu, 2, dtype> > project(seq_size);
    vector<Tensor<xpu, 2, dtype> > output(seq_size);

    vector<Tensor<xpu, 3, dtype> > charprime(seq_size);
    vector<Tensor<xpu, 3, dtype> > charinput(seq_size);
    vector<Tensor<xpu, 3, dtype> > charhidden(seq_size);
    vector<Tensor<xpu, 3, dtype> > chargatedpoolIndex(seq_size);
    vector<Tensor<xpu, 2, dtype> > chargatedpool(seq_size);
    vector<Tensor<xpu, 3, dtype> > chargateweightMiddle(seq_size);
    vector<Tensor<xpu, 3, dtype> > chargateweight(seq_size);
    vector<Tensor<xpu, 2, dtype> > chargateweightsum(seq_size);
    vector<Tensor<xpu, 3, dtype> > tagprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > tagoutput(seq_size);
    vector<Tensor<xpu, 2, dtype> > wordprime(seq_size);
    vector<Tensor<xpu, 2, dtype> > wordrepresent(seq_size);

    //initialize
    for (int idx = 0; idx < seq_size; idx++) {
      //basement params
      const Feature& base_feature = base_example.m_features[idx];

      int base_char_num = base_feature.chars.size();
      base_charprime[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_charDim), d_zero);
      base_charinput[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_inputSize), d_zero);
      base_charhidden[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargatedpoolIndex[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargatedpool[idx] = NewTensor<xpu>(Shape2(1, _base_char_outputSize), d_zero);
      base_chargateweightMiddle[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargateweight[idx] = NewTensor<xpu>(Shape3(base_char_num, 1, _base_char_outputSize), d_zero);
      base_chargateweightsum[idx] = NewTensor<xpu>(Shape2(1, _base_char_outputSize), d_zero);
      if (_base_tagNum > 0) {
        base_tagprime[idx] = NewTensor<xpu>(Shape3(_base_tagNum, 1, _base_tagDim[0]), d_zero);
        base_tagoutput[idx] = NewTensor<xpu>(Shape2(1, _base_tag_outputSize), d_zero);
      }
      base_wordprime[idx] = NewTensor<xpu>(Shape2(1, _base_wordDim), d_zero);
      base_wordrepresent[idx] = NewTensor<xpu>(Shape2(1, _base_token_representation_size), d_zero);

      base_input[idx] = NewTensor<xpu>(Shape2(1, _base_inputsize), d_zero);

      base_i_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_o_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_f_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_mc_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_c_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_my_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_project_left[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);

      base_i_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_o_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_f_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_mc_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_c_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_my_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);
      base_project_right[idx] = NewTensor<xpu>(Shape2(1, _base_lstmhiddensize), d_zero);

      base_lstmoutput[idx] = NewTensor<xpu>(Shape2(1, 2 * _base_lstmhiddensize), d_zero);
      base_project[idx] = NewTensor<xpu>(Shape2(1, _base_hiddensize), d_zero);
      base_output[idx] = NewTensor<xpu>(Shape2(1, _base_labelSize), d_zero);

      base2input[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);
      //secondary params
      const Feature& feature = example.m_features[idx];

      int char_num = feature.chars.size();
      charprime[idx] = NewTensor<xpu>(Shape3(char_num, 1, _charDim), d_zero);
      charinput[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_inputSize), d_zero);
      charhidden[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargatedpoolIndex[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargatedpool[idx] = NewTensor<xpu>(Shape2(1, _char_outputSize), d_zero);
      chargateweightMiddle[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargateweight[idx] = NewTensor<xpu>(Shape3(char_num, 1, _char_outputSize), d_zero);
      chargateweightsum[idx] = NewTensor<xpu>(Shape2(1, _char_outputSize), d_zero);
      if (_tagNum > 0) {
        tagprime[idx] = NewTensor<xpu>(Shape3(_tagNum, 1, _tagDim[0]), d_zero);
        tagoutput[idx] = NewTensor<xpu>(Shape2(1, _tag_outputSize), d_zero);
      }
      wordprime[idx] = NewTensor<xpu>(Shape2(1, _wordDim), d_zero);
      wordrepresent[idx] = NewTensor<xpu>(Shape2(1, _token_representation_size), d_zero);

      input[idx] = NewTensor<xpu>(Shape2(1, _inputsize), d_zero);

      i_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      o_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      f_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      mc_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      c_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      my_project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      project_left[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);

      i_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      o_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      f_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      mc_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      c_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      my_project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);
      project_right[idx] = NewTensor<xpu>(Shape2(1, _lstmhiddensize), d_zero);

      lstmoutput[idx] = NewTensor<xpu>(Shape2(1, 2 * _lstmhiddensize), d_zero);
      project[idx] = NewTensor<xpu>(Shape2(1, _hiddensize), d_zero);
      output[idx] = NewTensor<xpu>(Shape2(1, _labelSize), d_zero);
    }

    //forward propagation
    //basement input setting, and linear setting
    for (int idx = 0; idx < seq_size; idx++) {
      const Feature& base_feature = base_example.m_features[idx];
      //linear features should not be dropped out

      const vector<int>& base_words = base_feature.words;
      _base_words.GetEmb(base_words[0], base_wordprime[idx]);

      const vector<int>& base_chars = base_feature.chars;
      int base_char_num = base_chars.size();

      //charprime
      for (int idy = 0; idy < base_char_num; idy++) {
        _base_chars.GetEmb(base_chars[idy], base_charprime[idx][idy]);
      }

      // char context
      windowlized(base_charprime[idx], base_charinput[idx], _base_charcontext);

      // char combination
      _base_tanhchar_project.ComputeForwardScore(base_charinput[idx], base_charhidden[idx]);

      // char gated pooling
      _base_gatedchar_pooling.ComputeForwardScore(base_charhidden[idx], base_wordprime[idx], base_chargateweightMiddle[idx], base_chargateweight[idx], base_chargateweightsum[idx], base_chargatedpoolIndex[idx], base_chargatedpool[idx]);
      // tag prime get
      if (_base_tagNum > 0) {
        const vector<int>& base_tags = base_feature.tags;
        for (int idy = 0; idy < _base_tagNum; idy++){
          _base_tags[idy].GetEmb(base_tags[idy], base_tagprime[idx][idy]);
        }
        concat(base_tagprime[idx], base_tagoutput[idx]);
      }
    }
    if (_base_tagNum > 0) {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(base_wordprime[idx], base_chargatedpool[idx], base_tagoutput[idx], base_wordrepresent[idx]);
      }
    }
    else {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(base_wordprime[idx], base_chargatedpool[idx], base_wordrepresent[idx]);
      }
    }

    windowlized(base_wordrepresent, base_input, _base_wordcontext);

    base_rnn_left_project.ComputeForwardScore(base_input, base_i_project_left, base_o_project_left, base_f_project_left, base_mc_project_left,
        base_c_project_left, base_my_project_left, base_project_left);
    base_rnn_right_project.ComputeForwardScore(base_input, base_i_project_right, base_o_project_right, base_f_project_right, base_mc_project_right,
        base_c_project_right, base_my_project_right, base_project_right);

    for (int idx = 0; idx < seq_size; idx++) {
      concat(base_project_left[idx], base_project_right[idx], base_lstmoutput[idx]);
    }
    _base_tanh_project.ComputeForwardScore(base_lstmoutput, base_project);

    _base_olayer_linear.ComputeForwardScore(base_project, base_output);

    _baseinput_tanh_project.ComputeForwardScore(base_output, base2input);

    //input setting, and linear setting
    for (int idx = 0; idx < seq_size; idx++) {
      const Feature& feature = example.m_features[idx];
      //linear features should not be dropped out

      const vector<int>& words = feature.words;
      _words.GetEmb(words[0], wordprime[idx]);

      const vector<int>& chars = feature.chars;
      int char_num = chars.size();

      //charprime
      for (int idy = 0; idy < char_num; idy++) {
        _chars.GetEmb(chars[idy], charprime[idx][idy]);
      }

      // char context
      windowlized(charprime[idx], charinput[idx], _charcontext);

      // char combination
      _tanhchar_project.ComputeForwardScore(charinput[idx], charhidden[idx]);

      // char gated pooling
      _gatedchar_pooling.ComputeForwardScore(charhidden[idx], wordprime[idx], chargateweightMiddle[idx], chargateweight[idx], chargateweightsum[idx], chargatedpoolIndex[idx], chargatedpool[idx]);
      // tag prime get
      if (_tagNum > 0) {
        const vector<int>& tags = feature.tags;
        for (int idy = 0; idy < _tagNum; idy++){
          _tags[idy].GetEmb(tags[idy], tagprime[idx][idy]);
        }
        concat(tagprime[idx], tagoutput[idx]);  
      }
    }
    if (_tagNum > 0) {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(wordprime[idx], chargatedpool[idx], tagoutput[idx], base2input[idx], wordrepresent[idx]);
      }
    }
    else {
      for (int idx = 0; idx < seq_size; idx++) {
        concat(wordprime[idx], chargatedpool[idx], base2input[idx], wordrepresent[idx]);
      }      
    }

    windowlized(wordrepresent, input, _wordcontext);

    rnn_left_project.ComputeForwardScore(input, i_project_left, o_project_left, f_project_left, mc_project_left,
        c_project_left, my_project_left, project_left);
    rnn_right_project.ComputeForwardScore(input, i_project_right, o_project_right, f_project_right, mc_project_right,
        c_project_right, my_project_right, project_right);

    for (int idx = 0; idx < seq_size; idx++) {
      concat(project_left[idx], project_right[idx], lstmoutput[idx]);
    }
    _tanh_project.ComputeForwardScore(lstmoutput, project);

    _olayer_linear.ComputeForwardScore(project, output);

    // get delta for each output
    dtype cost = _crf_layer.cost(output, example.m_labels);

    //release
    for (int idx = 0; idx < seq_size; idx++) {
      FreeSpace(&(charprime[idx]));
      FreeSpace(&(charinput[idx]));
      FreeSpace(&(charhidden[idx]));
      FreeSpace(&(chargatedpoolIndex[idx]));
      FreeSpace(&(chargatedpool[idx]));
      FreeSpace(&(chargateweightMiddle[idx]));
      FreeSpace(&(chargateweight[idx]));
      FreeSpace(&(chargateweightsum[idx]));
      if (_tagNum > 0) {
        FreeSpace(&(tagprime[idx]));
        FreeSpace(&(tagoutput[idx]));
      }
      FreeSpace(&(wordprime[idx]));
      FreeSpace(&(wordrepresent[idx]));
      FreeSpace(&(input[idx]));

      FreeSpace(&(i_project_left[idx]));
      FreeSpace(&(o_project_left[idx]));
      FreeSpace(&(f_project_left[idx]));
      FreeSpace(&(mc_project_left[idx]));
      FreeSpace(&(c_project_left[idx]));
      FreeSpace(&(my_project_left[idx]));
      FreeSpace(&(project_left[idx]));

      FreeSpace(&(i_project_right[idx]));
      FreeSpace(&(o_project_right[idx]));
      FreeSpace(&(f_project_right[idx]));
      FreeSpace(&(mc_project_right[idx]));
      FreeSpace(&(c_project_right[idx]));
      FreeSpace(&(my_project_right[idx]));
      FreeSpace(&(project_right[idx]));

      FreeSpace(&(lstmoutput[idx]));
      FreeSpace(&(project[idx]));
      FreeSpace(&(output[idx]));
      FreeSpace(&(base2input[idx]));
      //release basement params
      FreeSpace(&(base_charprime[idx]));
      FreeSpace(&(base_charinput[idx]));
      FreeSpace(&(base_charhidden[idx]));
      FreeSpace(&(base_chargatedpoolIndex[idx]));
      FreeSpace(&(base_chargatedpool[idx]));
      FreeSpace(&(base_chargateweightMiddle[idx]));
      FreeSpace(&(base_chargateweight[idx]));
      FreeSpace(&(base_chargateweightsum[idx]));
      if (_base_tagNum > 0) {
        FreeSpace(&(base_tagprime[idx]));
        FreeSpace(&(base_tagoutput[idx]));
      }
      FreeSpace(&(base_wordprime[idx]));
      FreeSpace(&(base_wordrepresent[idx]));
      FreeSpace(&(base_input[idx]));

      FreeSpace(&(base_i_project_left[idx]));
      FreeSpace(&(base_o_project_left[idx]));
      FreeSpace(&(base_f_project_left[idx]));
      FreeSpace(&(base_mc_project_left[idx]));
      FreeSpace(&(base_c_project_left[idx]));
      FreeSpace(&(base_my_project_left[idx]));
      FreeSpace(&(base_project_left[idx]));

      FreeSpace(&(base_i_project_right[idx]));
      FreeSpace(&(base_o_project_right[idx]));
      FreeSpace(&(base_f_project_right[idx]));
      FreeSpace(&(base_mc_project_right[idx]));
      FreeSpace(&(base_c_project_right[idx]));
      FreeSpace(&(base_my_project_right[idx]));
      FreeSpace(&(base_project_right[idx]));

      FreeSpace(&(base_lstmoutput[idx]));
      FreeSpace(&(base_project[idx]));
      FreeSpace(&(base_output[idx]));
    }
    return cost;
  }

  void updateParams(dtype nnRegular, dtype adaAlpha, dtype adaEps) {
    rnn_left_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    rnn_right_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _tanhchar_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _gatedchar_pooling.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _tanh_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _olayer_linear.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _crf_layer.updateAdaGrad(nnRegular, adaAlpha, adaEps);

    _words.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _chars.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    for (int i = 0; i < _tagNum; i++){
      _tags[i].updateAdaGrad(nnRegular, adaAlpha, adaEps);
    }
    _baseinput_tanh_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    //update basement classifier
    base_rnn_left_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    base_rnn_right_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _base_tanhchar_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _base_gatedchar_pooling.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _base_tanh_project.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _base_olayer_linear.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    //_base_crf_layer.updateAdaGrad(nnRegular, adaAlpha, adaEps);

    _base_words.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    _base_chars.updateAdaGrad(nnRegular, adaAlpha, adaEps);
    for (int i = 0; i < _base_tagNum; i++){
      _base_tags[i].updateAdaGrad(nnRegular, adaAlpha, adaEps);
    }
  }

  void writeModel();

  void loadModel();

  void writeModel(LStream &outf) {
  //write basement model params
    WriteBinary(outf, _base_wordcontext);
    WriteBinary(outf, _base_wordwindow);
    WriteBinary(outf, _base_wordSize);
    WriteBinary(outf, _base_charcontext);
    WriteBinary(outf, _base_wordDim);
    WriteBinary(outf, _base_charwindow);
    WriteBinary(outf, _base_charSize);
    WriteBinary(outf, _base_charDim);
    WriteBinary(outf, _base_char_outputSize);
    WriteBinary(outf, _base_char_inputSize);
    WriteBinary(outf, _base_lstmhiddensize);
    WriteBinary(outf, _base_hiddensize);
    WriteBinary(outf, _base_inputsize);
    WriteBinary(outf, _base_token_representation_size);
    WriteBinary(outf, _base_labelSize);

    _base_words.writeModel(outf);
    _base_chars.writeModel(outf);
    WriteBinary(outf, _base_tagNum);
    if (_base_tagNum > 0) {
      WriteBinary(outf, _base_tag_outputSize);
      WriteVector(outf, _base_tagSize);
      WriteVector(outf, _base_tagDim);
      for (int idx = 0; idx < _base_tagNum; idx++) {
        _base_tags[idx].writeModel(outf);
      }
    }

    // cout << "dtype " << _dropOut <<endl;
    _base_crf_layer.writeModel(outf);

    _base_olayer_linear.writeModel(outf);
    _base_tanh_project.writeModel(outf);
    _base_tanhchar_project.writeModel(outf);
    base_rnn_left_project.writeModel(outf);
    base_rnn_right_project.writeModel(outf);
    _base_gatedchar_pooling.writeModel(outf);
    _base_eval.writeModel(outf);
    WriteBinary(outf, _base_dropOut);
  //write second model params
    _baseinput_tanh_project.writeModel(outf);
    WriteBinary(outf, _wordcontext);
    WriteBinary(outf, _wordwindow);
    WriteBinary(outf, _wordSize);
    WriteBinary(outf, _charcontext);
    WriteBinary(outf, _wordDim);
    WriteBinary(outf, _charwindow);
    WriteBinary(outf, _charSize);
    WriteBinary(outf, _charDim);
    WriteBinary(outf, _char_outputSize);
    WriteBinary(outf, _char_inputSize);
    WriteBinary(outf, _lstmhiddensize);
    WriteBinary(outf, _hiddensize);
    WriteBinary(outf, _inputsize);
    WriteBinary(outf, _token_representation_size);
    WriteBinary(outf, _labelSize);

    _words.writeModel(outf);
    _chars.writeModel(outf);
    WriteBinary(outf, _tagNum);
    if (_tagNum > 0) {
      WriteBinary(outf, _tag_outputSize);
      WriteVector(outf, _tagSize);
      WriteVector(outf, _tagDim);
      for (int idx = 0; idx < _tagNum; idx++) {
        _tags[idx].writeModel(outf);
      }
    }

    // cout << "dtype " << _dropOut <<endl;
    _crf_layer.writeModel(outf);

    _olayer_linear.writeModel(outf);
    _tanh_project.writeModel(outf);
    _tanhchar_project.writeModel(outf);
    rnn_left_project.writeModel(outf);
    rnn_right_project.writeModel(outf);
    _gatedchar_pooling.writeModel(outf);
    _eval.writeModel(outf);
    WriteBinary(outf, _dropOut);

  }

  void loadModel(LStream &inf) {
  //load basement model
    ReadBinary(inf, _base_wordcontext);
    ReadBinary(inf, _base_wordwindow);
    ReadBinary(inf, _base_wordSize);
    ReadBinary(inf, _base_charcontext);
    ReadBinary(inf, _base_wordDim);
    ReadBinary(inf, _base_charwindow);
    ReadBinary(inf, _base_charSize);
    ReadBinary(inf, _base_charDim);
    ReadBinary(inf, _base_char_outputSize);
    ReadBinary(inf, _base_char_inputSize);
    ReadBinary(inf, _base_lstmhiddensize);
    ReadBinary(inf, _base_hiddensize);
    ReadBinary(inf, _base_inputsize);
    ReadBinary(inf, _base_token_representation_size);
    ReadBinary(inf, _base_labelSize);

    _base_words.loadModel(inf);
    _base_chars.loadModel(inf);
    ReadBinary(inf, _base_tagNum);
    // cout << "tag Num " << _tagNum << endl;
    if (_base_tagNum > 0) {
      ReadBinary(inf, _base_tag_outputSize);
      _base_tags.resize(_base_tagNum);
      ReadVector(inf, _base_tagSize);
      ReadVector(inf, _base_tagDim);
      for (int idx = 0; idx < _base_tagNum; idx++) {
        _base_tags[idx].loadModel(inf);
      }
    }

    // cout << "dtype " << _dropOut <<endl;
    _base_crf_layer.loadModel(inf);

    _base_olayer_linear.loadModel(inf);
    _base_tanh_project.loadModel(inf);
    _base_tanhchar_project.loadModel(inf);
    base_rnn_left_project.loadModel(inf);
    base_rnn_right_project.loadModel(inf);
    _base_gatedchar_pooling.loadModel(inf);
    _base_eval.loadModel(inf);
    ReadBinary(inf, _base_dropOut);
  //load second model
    _baseinput_tanh_project.loadModel(inf);

    ReadBinary(inf, _wordcontext);
    ReadBinary(inf, _wordwindow);
    ReadBinary(inf, _wordSize);
    ReadBinary(inf, _charcontext);
    ReadBinary(inf, _wordDim);
    ReadBinary(inf, _charwindow);
    ReadBinary(inf, _charSize);
    ReadBinary(inf, _charDim);
    ReadBinary(inf, _char_outputSize);
    ReadBinary(inf, _char_inputSize);
    ReadBinary(inf, _lstmhiddensize);
    ReadBinary(inf, _hiddensize);
    ReadBinary(inf, _inputsize);
    ReadBinary(inf, _token_representation_size);
    ReadBinary(inf, _labelSize);

    _words.loadModel(inf);
    _chars.loadModel(inf);
    ReadBinary(inf, _tagNum);
    // cout << "tag Num " << _tagNum << endl;
    if (_tagNum > 0) {  
      ReadBinary(inf, _tag_outputSize);
      _tags.resize(_tagNum);  
      ReadVector(inf, _tagSize);
      ReadVector(inf, _tagDim);
      for (int idx = 0; idx < _tagNum; idx++) {
        _tags[idx].loadModel(inf);
      }
    }

    // cout << "dtype " << _dropOut <<endl;
    _crf_layer.loadModel(inf);

    _olayer_linear.loadModel(inf);
    _tanh_project.loadModel(inf);
    _tanhchar_project.loadModel(inf);
    rnn_left_project.loadModel(inf);
    rnn_right_project.loadModel(inf);
    _gatedchar_pooling.loadModel(inf);
    _eval.loadModel(inf);
    ReadBinary(inf, _dropOut);
  }
 void loadBaseModel(LStream &inf) {
    ReadBinary(inf, _base_wordcontext);
    ReadBinary(inf, _base_wordwindow);
    ReadBinary(inf, _base_wordSize);
    ReadBinary(inf, _base_charcontext);
    ReadBinary(inf, _base_wordDim);
    ReadBinary(inf, _base_charwindow);
    ReadBinary(inf, _base_charSize);
    ReadBinary(inf, _base_charDim);
    ReadBinary(inf, _base_char_outputSize);
    ReadBinary(inf, _base_char_inputSize);
    ReadBinary(inf, _base_lstmhiddensize);
    ReadBinary(inf, _base_hiddensize);
    ReadBinary(inf, _base_inputsize);
    ReadBinary(inf, _base_token_representation_size);
    ReadBinary(inf, _base_labelSize);

    _in_base_words.loadModel(inf);
    _base_chars.loadModel(inf);
    ReadBinary(inf, _base_tagNum);
    // cout << "tag Num " << _tagNum << endl;
    if (_base_tagNum > 0) {
      ReadBinary(inf, _base_tag_outputSize);
      _base_tags.resize(_base_tagNum);
      ReadVector(inf, _base_tagSize);
      ReadVector(inf, _base_tagDim);
      for (int idx = 0; idx < _base_tagNum; idx++) {
        _base_tags[idx].loadModel(inf);
      }
    }

    // cout << "dtype " << _dropOut <<endl;
    _base_crf_layer.loadModel(inf);

    _base_olayer_linear.loadModel(inf);
    _base_tanh_project.loadModel(inf);
    _base_tanhchar_project.loadModel(inf);
    base_rnn_left_project.loadModel(inf);
    base_rnn_right_project.loadModel(inf);
    _base_gatedchar_pooling.loadModel(inf);
    _base_eval.loadModel(inf);
    ReadBinary(inf, _base_dropOut);
  }

  void checkgrad(const vector<Example>& examples, const vector<Example>& base_examples,Tensor<xpu, 2, dtype> Wd, Tensor<xpu, 2, dtype> gradWd, const string& mark, int iter) {
    int charseed = mark.length();
    for (int i = 0; i < mark.length(); i++) {
      charseed = (int) (mark[i]) * 5 + charseed;
    }
    srand(iter + charseed);
    std::vector<int> idRows, idCols;
    idRows.clear();
    idCols.clear();
    for (int i = 0; i < Wd.size(0); ++i)
      idRows.push_back(i);
    for (int idx = 0; idx < Wd.size(1); idx++)
      idCols.push_back(idx);

    random_shuffle(idRows.begin(), idRows.end());
    random_shuffle(idCols.begin(), idCols.end());

    int check_i = idRows[0], check_j = idCols[0];

    dtype orginValue = Wd[check_i][check_j];

    Wd[check_i][check_j] = orginValue + 0.001;
    dtype lossAdd = 0.0;
    for (int i = 0; i < examples.size(); i++) {
      Example oneExam = examples[i];
      Example oneExam_base = base_examples[i];
      lossAdd += computeScore(oneExam, oneExam_base);
    }

    Wd[check_i][check_j] = orginValue - 0.001;
    dtype lossPlus = 0.0;
    for (int i = 0; i < examples.size(); i++) {
      Example oneExam = examples[i];
      Example oneExam_base = base_examples[i];
      lossPlus += computeScore(oneExam, oneExam_base);
    }

    dtype mockGrad = (lossAdd - lossPlus) / 0.002;
    mockGrad = mockGrad / examples.size();
    dtype computeGrad = gradWd[check_i][check_j];

    printf("Iteration %d, Checking gradient for %s[%d][%d]:\t", iter, mark.c_str(), check_i, check_j);
    printf("mock grad = %.18f, computed grad = %.18f\n", mockGrad, computeGrad);

    Wd[check_i][check_j] = orginValue;
  }

  void checkgrad(const vector<Example>& examples, const vector<Example>& base_examples,Tensor<xpu, 2, dtype> Wd, Tensor<xpu, 2, dtype> gradWd, const string& mark, int iter,
      const hash_set<int>& indexes, bool bRow = true) {
    int charseed = mark.length();
    for (int i = 0; i < mark.length(); i++) {
      charseed = (int) (mark[i]) * 5 + charseed;
    }
    srand(iter + charseed);
    std::vector<int> idRows, idCols;
    idRows.clear();
    idCols.clear();
    static hash_set<int>::iterator it;
    if (bRow) {
      for (it = indexes.begin(); it != indexes.end(); ++it)
        idRows.push_back(*it);
      for (int idx = 0; idx < Wd.size(1); idx++)
        idCols.push_back(idx);
    } else {
      for (it = indexes.begin(); it != indexes.end(); ++it)
        idCols.push_back(*it);
      for (int idx = 0; idx < Wd.size(0); idx++)
        idRows.push_back(idx);
    }

    random_shuffle(idRows.begin(), idRows.end());
    random_shuffle(idCols.begin(), idCols.end());

    int check_i = idRows[0], check_j = idCols[0];

    dtype orginValue = Wd[check_i][check_j];

    Wd[check_i][check_j] = orginValue + 0.001;
    dtype lossAdd = 0.0;
    for (int i = 0; i < examples.size(); i++) {
      Example oneExam = examples[i];
      Example oneExam_base = base_examples[i];
      lossAdd += computeScore(oneExam, oneExam_base);
    }

    Wd[check_i][check_j] = orginValue - 0.001;
    dtype lossPlus = 0.0;
    for (int i = 0; i < examples.size(); i++) {
      Example oneExam = examples[i];
      Example oneExam_base = base_examples[i];
      lossPlus += computeScore(oneExam, oneExam_base);
    }

    dtype mockGrad = (lossAdd - lossPlus) / 0.002;
    mockGrad = mockGrad / examples.size();
    dtype computeGrad = gradWd[check_i][check_j];

    printf("Iteration %d, Checking gradient for %s[%d][%d]:\t", iter, mark.c_str(), check_i, check_j);
    printf("mock grad = %.18f, computed grad = %.18f\n", mockGrad, computeGrad);

    Wd[check_i][check_j] = orginValue;

  }

  void checkgrads(const vector<Example>& examples, const vector<Example>& base_examples, int iter) {
    //check basement classifier
    checkgrad(examples, base_examples, _base_olayer_linear._W, _base_olayer_linear._gradW, "_base_olayer_linear._W", iter);
    checkgrad(examples, base_examples, _base_tanh_project._W, _base_tanh_project._gradW, "_base_tanh_project._W", iter);

    checkgrad(examples, base_examples, _base_tanhchar_project._W, _base_tanhchar_project._gradW, "_base_tanhchar_project._W", iter);
    checkgrad(examples, base_examples, _base_tanhchar_project._b, _base_tanhchar_project._gradb, "_base_tanhchar_project._b", iter);

    checkgrad(examples, base_examples, _base_gatedchar_pooling._bi_gates._WL, _base_gatedchar_pooling._bi_gates._gradWL, "_base_gatedchar_pooling._bi_gates._WL", iter);
    checkgrad(examples, base_examples, _base_gatedchar_pooling._bi_gates._WR, _base_gatedchar_pooling._bi_gates._gradWR, "_base_gatedchar_pooling._bi_gates._WR", iter);
    checkgrad(examples, base_examples, _base_gatedchar_pooling._bi_gates._b, _base_gatedchar_pooling._bi_gates._gradb, "_base_gatedchar_pooling._bi_gates._b", iter);

    checkgrad(examples, base_examples, _base_gatedchar_pooling._uni_gates._W, _base_gatedchar_pooling._uni_gates._gradW, "_base_gatedchar_pooling._uni_gates._W", iter);
    checkgrad(examples, base_examples, _base_gatedchar_pooling._uni_gates._b, _base_gatedchar_pooling._uni_gates._gradb, "_base_gatedchar_pooling._uni_gates._b", iter);

    checkgrad(examples, base_examples, base_rnn_left_project._lstm_output._W1, base_rnn_left_project._lstm_output._gradW1, "base_rnn_left_project._lstm_output._W1", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_output._W2, base_rnn_left_project._lstm_output._gradW2, "base_rnn_left_project._lstm_output._W2", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_output._W3, base_rnn_left_project._lstm_output._gradW3, "base_rnn_left_project._lstm_output._W3", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_output._b, base_rnn_left_project._lstm_output._gradb, "base_rnn_left_project._lstm_output._b", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_input._W1, base_rnn_left_project._lstm_input._gradW1, "base_rnn_left_project._lstm_input._W1", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_input._W2, base_rnn_left_project._lstm_input._gradW2, "base_rnn_left_project._lstm_input._W2", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_input._W3, base_rnn_left_project._lstm_input._gradW3, "base_rnn_left_project._lstm_input._W3", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_input._b, base_rnn_left_project._lstm_input._gradb, "base_rnn_left_project._lstm_input._b", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_forget._W1, base_rnn_left_project._lstm_forget._gradW1, "base_rnn_left_project._lstm_forget._W1", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_forget._W2, base_rnn_left_project._lstm_forget._gradW2, "base_rnn_left_project._lstm_forget._W2", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_forget._W3, base_rnn_left_project._lstm_forget._gradW3, "base_rnn_left_project._lstm_forget._W3", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_forget._b, base_rnn_left_project._lstm_forget._gradb, "base_rnn_left_project._lstm_forget._b", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_cell._WL, base_rnn_left_project._lstm_cell._gradWL, "base_rnn_left_project._lstm_cell._WL", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_cell._WR, base_rnn_left_project._lstm_cell._gradWR, "base_rnn_left_project._lstm_cell._WR", iter);
    checkgrad(examples, base_examples, base_rnn_left_project._lstm_cell._b, base_rnn_left_project._lstm_cell._gradb, "base_rnn_left_project._lstm_cell._b", iter);


    checkgrad(examples, base_examples, base_rnn_right_project._lstm_output._W1, base_rnn_right_project._lstm_output._gradW1, "base_rnn_right_project._lstm_output._W1", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_output._W2, base_rnn_right_project._lstm_output._gradW2, "base_rnn_right_project._lstm_output._W2", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_output._W3, base_rnn_right_project._lstm_output._gradW3, "base_rnn_right_project._lstm_output._W3", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_output._b, base_rnn_right_project._lstm_output._gradb, "base_rnn_right_project._lstm_output._b", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_input._W1, base_rnn_right_project._lstm_input._gradW1, "base_rnn_right_project._lstm_input._W1", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_input._W2, base_rnn_right_project._lstm_input._gradW2, "base_rnn_right_project._lstm_input._W2", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_input._W3, base_rnn_right_project._lstm_input._gradW3, "base_rnn_right_project._lstm_input._W3", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_input._b, base_rnn_right_project._lstm_input._gradb, "base_rnn_right_project._lstm_input._b", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_forget._W1, base_rnn_right_project._lstm_forget._gradW1, "base_rnn_right_project._lstm_forget._W1", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_forget._W2, base_rnn_right_project._lstm_forget._gradW2, "base_rnn_right_project._lstm_forget._W2", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_forget._W3, base_rnn_right_project._lstm_forget._gradW3, "base_rnn_right_project._lstm_forget._W3", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_forget._b, base_rnn_right_project._lstm_forget._gradb, "base_rnn_right_project._lstm_forget._b", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_cell._WL, base_rnn_right_project._lstm_cell._gradWL, "base_rnn_right_project._lstm_cell._WL", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_cell._WR, base_rnn_right_project._lstm_cell._gradWR, "base_rnn_right_project._lstm_cell._WR", iter);
    checkgrad(examples, base_examples, base_rnn_right_project._lstm_cell._b, base_rnn_right_project._lstm_cell._gradb, "base_rnn_right_project._lstm_cell._b", iter);

    checkgrad(examples, base_examples, _base_crf_layer._tagBigram, _base_crf_layer._grad_tagBigram, "_base_crf_layer._tagBigram", iter);

    checkgrad(examples, base_examples, _base_words._E, _base_words._gradE, "_base_words._E", iter, _base_words._indexers);
    checkgrad(examples, base_examples, _base_chars._E, _base_chars._gradE, "_base_chars._E", iter, _base_chars._indexers);
    // tag checkgrad

    for (int i = 0; i < _base_tagNum; i++){
      checkgrad(examples, base_examples, _base_tags[i]._E, _base_tags[i]._gradE, "_base_tags._E", iter, _base_tags[i]._indexers);
    }
    checkgrad(examples, base_examples, _baseinput_tanh_project._W, _baseinput_tanh_project._gradW, "_baseinput_tanh_project._W", iter);
    checkgrad(examples, base_examples, _baseinput_tanh_project._b, _baseinput_tanh_project._gradb, "_baseinput_tanh_project._b", iter);
    //check second classifier
    checkgrad(examples, base_examples, _olayer_linear._W, _olayer_linear._gradW, "_olayer_linear._W", iter);
    checkgrad(examples, base_examples, _tanh_project._W, _tanh_project._gradW, "_tanh_project._W", iter);

    checkgrad(examples, base_examples, _tanhchar_project._W, _tanhchar_project._gradW, "_tanhchar_project._W", iter);
    checkgrad(examples, base_examples, _tanhchar_project._b, _tanhchar_project._gradb, "_tanhchar_project._b", iter);

    checkgrad(examples, base_examples, _gatedchar_pooling._bi_gates._WL, _gatedchar_pooling._bi_gates._gradWL, "_gatedchar_pooling._bi_gates._WL", iter);
    checkgrad(examples, base_examples, _gatedchar_pooling._bi_gates._WR, _gatedchar_pooling._bi_gates._gradWR, "_gatedchar_pooling._bi_gates._WR", iter);
    checkgrad(examples, base_examples, _gatedchar_pooling._bi_gates._b, _gatedchar_pooling._bi_gates._gradb, "_gatedchar_pooling._bi_gates._b", iter);

    checkgrad(examples, base_examples, _gatedchar_pooling._uni_gates._W, _gatedchar_pooling._uni_gates._gradW, "_gatedchar_pooling._uni_gates._W", iter);
    checkgrad(examples, base_examples, _gatedchar_pooling._uni_gates._b, _gatedchar_pooling._uni_gates._gradb, "_gatedchar_pooling._uni_gates._b", iter);

    checkgrad(examples, base_examples, rnn_left_project._lstm_output._W1, rnn_left_project._lstm_output._gradW1, "rnn_left_project._lstm_output._W1", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_output._W2, rnn_left_project._lstm_output._gradW2, "rnn_left_project._lstm_output._W2", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_output._W3, rnn_left_project._lstm_output._gradW3, "rnn_left_project._lstm_output._W3", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_output._b, rnn_left_project._lstm_output._gradb, "rnn_left_project._lstm_output._b", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_input._W1, rnn_left_project._lstm_input._gradW1, "rnn_left_project._lstm_input._W1", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_input._W2, rnn_left_project._lstm_input._gradW2, "rnn_left_project._lstm_input._W2", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_input._W3, rnn_left_project._lstm_input._gradW3, "rnn_left_project._lstm_input._W3", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_input._b, rnn_left_project._lstm_input._gradb, "rnn_left_project._lstm_input._b", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_forget._W1, rnn_left_project._lstm_forget._gradW1, "rnn_left_project._lstm_forget._W1", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_forget._W2, rnn_left_project._lstm_forget._gradW2, "rnn_left_project._lstm_forget._W2", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_forget._W3, rnn_left_project._lstm_forget._gradW3, "rnn_left_project._lstm_forget._W3", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_forget._b, rnn_left_project._lstm_forget._gradb, "rnn_left_project._lstm_forget._b", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_cell._WL, rnn_left_project._lstm_cell._gradWL, "rnn_left_project._lstm_cell._WL", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_cell._WR, rnn_left_project._lstm_cell._gradWR, "rnn_left_project._lstm_cell._WR", iter);
    checkgrad(examples, base_examples, rnn_left_project._lstm_cell._b, rnn_left_project._lstm_cell._gradb, "rnn_left_project._lstm_cell._b", iter);


    checkgrad(examples, base_examples, rnn_right_project._lstm_output._W1, rnn_right_project._lstm_output._gradW1, "rnn_right_project._lstm_output._W1", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_output._W2, rnn_right_project._lstm_output._gradW2, "rnn_right_project._lstm_output._W2", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_output._W3, rnn_right_project._lstm_output._gradW3, "rnn_right_project._lstm_output._W3", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_output._b, rnn_right_project._lstm_output._gradb, "rnn_right_project._lstm_output._b", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_input._W1, rnn_right_project._lstm_input._gradW1, "rnn_right_project._lstm_input._W1", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_input._W2, rnn_right_project._lstm_input._gradW2, "rnn_right_project._lstm_input._W2", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_input._W3, rnn_right_project._lstm_input._gradW3, "rnn_right_project._lstm_input._W3", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_input._b, rnn_right_project._lstm_input._gradb, "rnn_right_project._lstm_input._b", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_forget._W1, rnn_right_project._lstm_forget._gradW1, "rnn_right_project._lstm_forget._W1", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_forget._W2, rnn_right_project._lstm_forget._gradW2, "rnn_right_project._lstm_forget._W2", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_forget._W3, rnn_right_project._lstm_forget._gradW3, "rnn_right_project._lstm_forget._W3", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_forget._b, rnn_right_project._lstm_forget._gradb, "rnn_right_project._lstm_forget._b", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_cell._WL, rnn_right_project._lstm_cell._gradWL, "rnn_right_project._lstm_cell._WL", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_cell._WR, rnn_right_project._lstm_cell._gradWR, "rnn_right_project._lstm_cell._WR", iter);
    checkgrad(examples, base_examples, rnn_right_project._lstm_cell._b, rnn_right_project._lstm_cell._gradb, "rnn_right_project._lstm_cell._b", iter);

    checkgrad(examples, base_examples, _crf_layer._tagBigram, _crf_layer._grad_tagBigram, "_crf_layer._tagBigram", iter);

    checkgrad(examples, base_examples, _words._E, _words._gradE, "_words._E", iter, _words._indexers);
    checkgrad(examples, base_examples, _chars._E, _chars._gradE, "_chars._E", iter, _chars._indexers);
    // tag checkgrad
    
    for (int i = 0; i < _tagNum; i++){
      checkgrad(examples, base_examples, _tags[i]._E, _tags[i]._gradE, "_tags._E", iter, _tags[i]._indexers);
    }

  }

public:
  inline void resetEval() {
    _eval.reset();
  }

  inline void setDropValue(dtype dropOut) {
    _dropOut = dropOut;
  }

  inline void setWordEmbFinetune(bool b_wordEmb_finetune) {
    _words.setEmbFineTune(b_wordEmb_finetune);
  }
  
  inline void setTagEmbFinetune(bool b_tagEmb_finetune) {
    for (int idx = 0; idx < _tagNum; idx++){
      _tags[idx].setEmbFineTune(b_tagEmb_finetune);
    }   
  }


};

#endif /* SRC_LSTMCRFMMClassifier_H_ */
