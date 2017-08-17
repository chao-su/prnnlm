///////////////////////////////////////////////////////////////////////
//
// A parallel recurrent neural network for language modeling
// Developed based on the following toolkit
//
///////////////////////////////////////////////////////////////////////
//
// Recurrent neural network based statistical language modeling toolkit
// Version 0.4a
// (c) 2010-2012 Tomas Mikolov (tmikolov@gmail.com)
// (c) 2013 Cantab Research Ltd (info@cantabResearch.com)
//
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <cfloat>
#include "fastexp.h"
#include "rnnlmlib.h"
//prnnlm
#include <iostream>
using namespace std;
//end prnnlm

///// include blas
#ifdef USE_BLAS
extern "C" {
#include <cblas.h>
}
#endif
//


real CRnnLM::random(real min, real max)
{
    return rand()/(real)RAND_MAX*(max-min)+min;
}

void CRnnLM::setTrainFile(char *str)
{
    strcpy(train_file, str);
}

void CRnnLM::setValidFile(char *str)
{
    strcpy(valid_file, str);
}

void CRnnLM::setTestFile(char *str)
{
    strcpy(test_file, str);
}

void CRnnLM::setRnnLMFile(char *str)
{
    strcpy(rnnlm_file, str);
}

void CRnnLM::readWord(char *word, FILE *fin)
{
    int a=0, ch;

    while (!feof(fin)) {
	ch=fgetc(fin);

	if (ch==13) continue;
       
        //prnnlm: read latent variable
        if (ch=='|') {
            ch=fgetc(fin);
            last_var=var;
            var=0;
            while(!feof(fin) && ch!='|') {
                var=var*10 + (int)(ch-48);
                ch=fgetc(fin);
            }
            var_update=true;
            continue;
        }
        //end prnnlm

	if ((ch==' ') || (ch=='\t') || (ch=='\n')) {
    	    if (a>0) {
                if (ch=='\n') ungetc(ch, fin);
                break;
            }
            if (ch=='\n') {
                strcpy(word, (char *)"</s>");
                return;
                continue;
            }
            else continue;
        }

        word[a]=ch;
        a++;

        if (a>=MAX_STRING) {
            //printf("Too long word found!\n");   //truncate too long words
            a--;
        }
    }
    word[a]=0;
	//cout<<word<<endl;	//prnnlm
}

int CRnnLM::getWordHash(char *word)
{
    unsigned int hash, a;
    
    hash=0;
    for (a=0; a<strlen(word); a++) hash=hash*237+word[a];
    hash=hash%vocab_hash_size;
    
    return hash;
}

int CRnnLM::searchVocab(char *word)
{
    int a;
    unsigned int hash;
    
    hash=getWordHash(word);
    
    if (vocab_hash[hash]==-1) return -1;
    if (!strcmp(word, vocab[vocab_hash[hash]].word)) return vocab_hash[hash];
    
    for (a=0; a<vocab_size; a++) {				//search in vocabulary
        if (!strcmp(word, vocab[a].word)) {
    	    vocab_hash[hash]=a;
    	    return a;
    	}
    }

    return -1;							//return OOV if not found
}

int CRnnLM::readWordIndex(FILE *fin)
{
    char word[MAX_STRING];

    readWord(word, fin);
    if (feof(fin)) return -1;
    
    return searchVocab(word);
}

int CRnnLM::addWordToVocab(char *word)
{
    unsigned int hash;
    
    strcpy(vocab[vocab_size].word, word);
    vocab[vocab_size].cn=0;
    vocab_size++;

    if (vocab_size+2>=vocab_max_size) {        //reallocate memory if needed
        vocab_max_size+=100;
        vocab=(struct vocab_word *)realloc(vocab, vocab_max_size * sizeof(struct vocab_word));
    }
    
    hash=getWordHash(word);
    vocab_hash[hash]=vocab_size-1;

    return vocab_size-1;
}

void CRnnLM::sortVocab()
{
    int a, b, max;
    vocab_word swap;
    
    for (a=1; a<vocab_size; a++) {
        max=a;
        for (b=a+1; b<vocab_size; b++) if (vocab[max].cn<vocab[b].cn) max=b;

        swap=vocab[max];
        vocab[max]=vocab[a];
        vocab[a]=swap;
    }
}

void CRnnLM::learnVocabFromTrainFile()    //assumes that vocabulary is empty
{
    char word[MAX_STRING];
    FILE *fin;
    int a, i, train_wcn;
    
    for (a=0; a<vocab_hash_size; a++) vocab_hash[a]=-1;

    fin=fopen(train_file, "rb");

    vocab_size=0;

    addWordToVocab((char *)"</s>");

    train_wcn=0;
    while (1) {
        readWord(word, fin);
        if (feof(fin)) break;
        
        train_wcn++;

        i=searchVocab(word);
        if (i==-1) {
            a=addWordToVocab(word);
            vocab[a].cn=1;
        } else vocab[i].cn++;
    }

    sortVocab();
    
    //select vocabulary size
    /*a=0;
    while (a<vocab_size) {
	a++;
	if (vocab[a].cn==0) break;
    }
    vocab_size=a;*/

    if (debug_mode>0) {
		printf("Vocab size: %d\n", vocab_size);
		printf("Words in train file: %d\n", train_wcn);
    }
    
    train_words=train_wcn;

    fclose(fin);
}

void CRnnLM::saveWeights()      //saves current weights and unit activations
{
    int a,b;

    for (a=0; a<layer0_size; a++) {
        neu0b[a].ac=neu0[a].ac;
        neu0b[a].er=neu0[a].er;
    }

    for (a=0; a<layer1_size; a++) {
        neu1b[a].ac=neu1[a].ac;
        neu1b[a].er=neu1[a].er;
    }
    
    for (a=0; a<layerc_size; a++) {
        neucb[a].ac=neuc[a].ac;
        neucb[a].er=neuc[a].er;
    }
    
    for (a=0; a<layer2_size; a++) {
        neu2b[a].ac=neu2[a].ac;
        neu2b[a].er=neu2[a].er;
    }
    
    for (b=0; b<layer1_size; b++) for (a=0; a<layer0_size; a++) {
		syn0b[a+b*layer0_size].weight=syn0[a+b*layer0_size].weight;
    }
    
    if (layerc_size>0) {
	for (b=0; b<layerc_size; b++) for (a=0; a<layer1_size; a++) {
	    syn1b[a+b*layer1_size].weight=syn1[a+b*layer1_size].weight;
	}
	
	for (b=0; b<layer2_size; b++) for (a=0; a<layerc_size; a++) {
	    syncb[a+b*layerc_size].weight=sync[a+b*layerc_size].weight;
	}
    }
    else {
	for (b=0; b<layer2_size; b++) for (a=0; a<layer1_size; a++) {
	    syn1b[a+b*layer1_size].weight=syn1[a+b*layer1_size].weight;
	}
    }
    
    //for (a=0; a<direct_size; a++) syn_db[a].weight=syn_d[a].weight;
}

void CRnnLM::saveVarWeights()      //saves current weights and unit activations
{
    int a,b;

    //prnnlm
    for (a=0; a<var_size; a++) {
        neuvb[a].ac=neuv[a].ac;
        neuvb[a].er=neuv[a].er;
        neuvvb[a].ac=neuvv[a].ac;
        neuvvb[a].er=neuvv[a].er;
    }
	for (a=var_size; a<var_size+varh_size; a++) {
		neuvvb[a].ac = neuvv[a].ac;
		neuvvb[a].er = neuvv[a].er;
	}
	for (a=0; a<varh_size; a++) {
		neuvhb[a].ac = neuvh[a].ac;
		neuvhb[a].er = neuvh[a].er;
	}
    if (layerc_size>0)
        for (b=0; b<var_size; b++) for (a=0; a<layerc_size; a++)
            syn2b[a+b*layerc_size].weight=syn2[a+b*layerc_size].weight;
    else
        for (b=0; b<var_size; b++) for (a=0; a<layer1_size; a++)
            syn2b[a+b*layer1_size].weight=syn2[a+b*layer1_size].weight;
    //for (b=0; b<var_size; b++) for (a=0; a<var_size; a++)
    //    syn3b[a+b*var_size].weight=syn3[a+b*var_size].weight;
	for (b=0; b<var_size+varh_size; b++) for (a=0; a<varh_size; a++)
        syn3b[a+b*varh_size].weight=syn3[a+b*varh_size].weight;
	for (b=0; b<varh_size; b++) for (a=0; a<var_size; a++)
        syn5b[a+b*var_size].weight=syn5[a+b*var_size].weight;
    for (b=0; b<layer2_size; b++) for (a=0; a<var_size; a++)
        syn4b[a+b*var_size].weight=syn4[a+b*var_size].weight;
	for (b=0; b<layer2_size; b++) for (a=0; a<varh_size; a++)
        syn6b[a+b*varh_size].weight=syn6[a+b*varh_size].weight;
	for (b=0; b<layer1_size; b++) for (a=0; a<varh_size; a++)
        syn7b[a+b*varh_size].weight=syn7[a+b*varh_size].weight;
	for (b=0; b<layer1_size; b++) for (a=0; a<var_size; a++)
        syn8b[a+b*var_size].weight=syn8[a+b*var_size].weight;
    //end prnnlm
}

void CRnnLM::restoreWeights()      //restores current weights and unit activations from backup copy
{
    int a,b;

    for (a=0; a<layer0_size; a++) {
        neu0[a].ac=neu0b[a].ac;
        neu0[a].er=neu0b[a].er;
    }

    for (a=0; a<layer1_size; a++) {
        neu1[a].ac=neu1b[a].ac;
        neu1[a].er=neu1b[a].er;
    }
    
    for (a=0; a<layerc_size; a++) {
        neuc[a].ac=neucb[a].ac;
        neuc[a].er=neucb[a].er;
    }
    
    for (a=0; a<layer2_size; a++) {
        neu2[a].ac=neu2b[a].ac;
        neu2[a].er=neu2b[a].er;
    }

    for (b=0; b<layer1_size; b++) for (a=0; a<layer0_size; a++) {
        syn0[a+b*layer0_size].weight=syn0b[a+b*layer0_size].weight;
    }
    
    if (layerc_size>0) {
	for (b=0; b<layerc_size; b++) for (a=0; a<layer1_size; a++) {
	    syn1[a+b*layer1_size].weight=syn1b[a+b*layer1_size].weight;
	}
	
	for (b=0; b<layer2_size; b++) for (a=0; a<layerc_size; a++) {
	    sync[a+b*layerc_size].weight=syncb[a+b*layerc_size].weight;
	}
    }
    else {
	for (b=0; b<layer2_size; b++) for (a=0; a<layer1_size; a++) {
	    syn1[a+b*layer1_size].weight=syn1b[a+b*layer1_size].weight;
	}
    }
    
    //for (a=0; a<direct_size; a++) syn_d[a].weight=syn_db[a].weight;
}

void CRnnLM::restoreVarWeights()      //restores current weights and unit activations from backup copy
{
    int a,b;

    //prnnlm
    for (a=0; a<var_size; a++) {
        neuv[a].ac=neuvb[a].ac;
        neuv[a].er=neuvb[a].er;
        neuvv[a].ac=neuvvb[a].ac;
        neuvv[a].er=neuvvb[a].er;
    }
	for (a=var_size; a<var_size+varh_size; a++) {
		neuvv[a].ac = neuvvb[a].ac;
		neuvv[a].er = neuvvb[a].er;
	}
	for (a=0; a<varh_size; a++) {
		neuvh[a].ac = neuvhb[a].ac;
		neuvh[a].er = neuvhb[a].er;
	}
    if (layerc_size>0)
        for (b=0; b<var_size; b++) for (a=0; a<layerc_size; a++)
            syn2[a+b*layerc_size].weight=syn2b[a+b*layerc_size].weight;
    else
        for (b=0; b<var_size; b++) for (a=0; a<layer1_size; a++)
            syn2[a+b*layer1_size].weight=syn2b[a+b*layer1_size].weight;
    //for (b=0; b<var_size; b++) for (a=0; a<var_size; a++)
    //    syn3[a+b*var_size].weight=syn3b[a+b*var_size].weight;
	for (b=0; b<var_size+varh_size; b++) for (a=0; a<varh_size; a++)
        syn3[a+b*varh_size].weight=syn3b[a+b*varh_size].weight;
	for (b=0; b<varh_size; b++) for (a=0; a<var_size; a++)
        syn5[a+b*var_size].weight=syn5b[a+b*var_size].weight;
    for (b=0; b<layer2_size; b++) for (a=0; a<var_size; a++)
        syn4[a+b*var_size].weight=syn4b[a+b*var_size].weight;
	for (b=0; b<layer2_size; b++) for (a=0; a<varh_size; a++)
        syn6[a+b*varh_size].weight=syn6b[a+b*varh_size].weight;
	for (b=0; b<layer1_size; b++) for (a=0; a<varh_size; a++)
        syn7[a+b*varh_size].weight=syn7b[a+b*varh_size].weight;
	for (b=0; b<layer1_size; b++) for (a=0; a<var_size; a++)
        syn8[a+b*var_size].weight=syn8b[a+b*var_size].weight;
    //end prnnlm 
}

void CRnnLM::saveContext()		//useful for n-best list processing
{
    int a;
    
    for (a=0; a<layer1_size; a++) neu1b[a].ac=neu1[a].ac;
}

void CRnnLM::restoreContext()
{
    int a;
    
    for (a=0; a<layer1_size; a++) neu1[a].ac=neu1b[a].ac;
}

void CRnnLM::saveContext2()
{
    int a;
    
    for (a=0; a<layer1_size; a++) neu1b2[a].ac=neu1[a].ac;
}

void CRnnLM::restoreContext2()
{
    int a;
    
    for (a=0; a<layer1_size; a++) neu1[a].ac=neu1b2[a].ac;
}

void CRnnLM::initNet()
{
    int a, b, cl;

    layer0_size=vocab_size+layer1_size;
    layer2_size=vocab_size+class_size;

    neu0=(struct neuron *)calloc(layer0_size, sizeof(struct neuron));
    neu1=(struct neuron *)calloc(layer1_size, sizeof(struct neuron));
    neuc=(struct neuron *)calloc(layerc_size, sizeof(struct neuron));
    neu2=(struct neuron *)calloc(layer2_size, sizeof(struct neuron));
    
    syn0=(struct synapse *)calloc(layer0_size*layer1_size, sizeof(struct synapse));
    //prnnlm
    var=-1;
    last_var=-1;
    var_update=false;
    neuv=(struct neuron *)calloc(var_size, sizeof(struct neuron));
    neuvb=(struct neuron *)calloc(var_size, sizeof(struct neuron));
    neuvv=(struct neuron *)calloc(var_size, sizeof(struct neuron));
    neuvvb=(struct neuron *)calloc(var_size, sizeof(struct neuron));
	neuvh=(struct neuron *)calloc(varh_size, sizeof(struct neuron));
	neuvhb=(struct neuron *)calloc(varh_size, sizeof(struct neuron));
    syn3=(struct synapse *)calloc((var_size+varh_size)*var_size, sizeof(struct synapse));
    syn3b=(struct synapse *)calloc((var_size+varh_size)*var_size, sizeof(struct synapse));
    syn4=(struct synapse *)calloc(var_size*layer2_size, sizeof(struct synapse));
    syn4b=(struct synapse *)calloc(var_size*layer2_size, sizeof(struct synapse));
	syn5=(struct synapse *)calloc(varh_size*var_size, sizeof(struct synapse));
    syn5b=(struct synapse *)calloc(varh_size*var_size, sizeof(struct synapse));
	syn6=(struct synapse *)calloc(varh_size*layer2_size, sizeof(struct synapse));
    syn6b=(struct synapse *)calloc(varh_size*layer2_size, sizeof(struct synapse));
	syn7=(struct synapse *)calloc(varh_size*layer1_size, sizeof(struct synapse));
    syn7b=(struct synapse *)calloc(varh_size*layer1_size, sizeof(struct synapse));
	syn8=(struct synapse *)calloc(var_size*layer1_size, sizeof(struct synapse));
    syn8b=(struct synapse *)calloc(var_size*layer1_size, sizeof(struct synapse));
    //end prnnlm
    if (layerc_size==0) {
	syn1=(struct synapse *)calloc(layer1_size*layer2_size, sizeof(struct synapse));
        syn2=(struct synapse *)calloc(layer1_size*var_size, sizeof(struct synapse)); //prnnlm
        syn2b=(struct synapse *)calloc(layer1_size*var_size, sizeof(struct synapse)); //prnnlm
    }
    else {
	syn1=(struct synapse *)calloc(layer1_size*layerc_size, sizeof(struct synapse));
	sync=(struct synapse *)calloc(layerc_size*layer2_size, sizeof(struct synapse));
        syn2=(struct synapse *)calloc(layerc_size*var_size, sizeof(struct synapse)); //prnnlm
        syn2b=(struct synapse *)calloc(layerc_size*var_size, sizeof(struct synapse)); //prnnlm
    }

    //if (syn1==NULL) {
    if (syn1==NULL || syn2==NULL || syn3==NULL || syn4==NULL) { //prnnlm
	printf("Memory allocation failed\n");
	exit(1);
    }
    
    if (layerc_size>0) if (sync==NULL) {
	printf("Memory allocation failed\n");
	exit(1);
    }
    
    syn_d=(direct_t *)calloc((long long)direct_size, sizeof(direct_t));

    if (syn_d==NULL) {
	printf("Memory allocation for direct connections failed (requested %lld bytes)\n", (long long)direct_size * (long long)sizeof(direct_t));
	exit(1);
    }

    neu0b=(struct neuron *)calloc(layer0_size, sizeof(struct neuron));
    neu1b=(struct neuron *)calloc(layer1_size, sizeof(struct neuron));
    neucb=(struct neuron *)calloc(layerc_size, sizeof(struct neuron));
    neu1b2=(struct neuron *)calloc(layer1_size, sizeof(struct neuron));
    neu2b=(struct neuron *)calloc(layer2_size, sizeof(struct neuron));

    syn0b=(struct synapse *)calloc(layer0_size*layer1_size, sizeof(struct synapse));
    //syn1b=(struct synapse *)calloc(layer1_size*layer2_size, sizeof(struct synapse));
    if (layerc_size==0)
	syn1b=(struct synapse *)calloc(layer1_size*layer2_size, sizeof(struct synapse));
    else {
	syn1b=(struct synapse *)calloc(layer1_size*layerc_size, sizeof(struct synapse));
	syncb=(struct synapse *)calloc(layerc_size*layer2_size, sizeof(struct synapse));
    }

    if (syn1b==NULL) {
	printf("Memory allocation failed\n");
	exit(1);
    }
    
    for (a=0; a<layer0_size; a++) {
        neu0[a].ac=0;
        neu0[a].er=0;
    }

    for (a=0; a<layer1_size; a++) {
        neu1[a].ac=0;
        neu1[a].er=0;
    }
    
    for (a=0; a<layerc_size; a++) {
        neuc[a].ac=0;
        neuc[a].er=0;
    }
    
    for (a=0; a<layer2_size; a++) {
        neu2[a].ac=0;
        neu2[a].er=0;
    } 

    for (b=0; b<layer1_size; b++) for (a=0; a<layer0_size; a++) {
        syn0[a+b*layer0_size].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
    }

    if (layerc_size>0) {
	for (b=0; b<layerc_size; b++) for (a=0; a<layer1_size; a++) {
	    syn1[a+b*layer1_size].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
	}
	
	for (b=0; b<layer2_size; b++) for (a=0; a<layerc_size; a++) {
	    sync[a+b*layerc_size].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
	}
       
        //prnnlm
        for (a=0; a<var_size*layerc_size; a++)
            syn2[a].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
        //end prnnlm
    }
    else {
	for (b=0; b<layer2_size; b++) for (a=0; a<layer1_size; a++) {
	    syn1[a+b*layer1_size].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
	}
        //prnnlm
        for (a=0; a<var_size*layer1_size; a++)
            syn2[a].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
        //end prnnlm
    }

    //prnnlm
    for (a=0; a<var_size; a++) {
        neuv[a].ac=0;
        neuv[a].er=0;
        neuvv[a].ac=0;
        neuvv[a].er=0;
        for (b=0; b<layer2_size; b++)
            syn4[b+a*layer2_size].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
    }
	for(a=0; a<(var_size+varh_size); a++) {
		for (b=0; b<varh_size; b++)
			syn3[b+a*varh_size].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
	}
	for(a=0; a<varh_size; a++) {
		neuvh[a].ac=0;
		neuvh[a].er=0;
		for (b=0; b<var_size; b++)
			syn5[b+a*var_size].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
	}
	for(a=var_size; a<var_size+varh_size; a++) {
		neuvv[a].ac=0;
		neuvv[a].er=0;
	}
	for(a=0; a<varh_size*layer2_size; a++)
		syn6[a].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
	for(a=0; a<varh_size*layer1_size; a++)
		syn7[a].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
	for(a=0; a<var_size*layer1_size; a++)
		syn8[a].weight=random(-0.1, 0.1)+random(-0.1, 0.1)+random(-0.1, 0.1);
    //end prnnlm
    
    long long aa;
    for (aa=0; aa<direct_size; aa++) syn_d[aa]=0;
    
    if (bptt>0) {
	bptt_history=(int *)calloc((bptt+bptt_block+10), sizeof(int));
	for (a=0; a<bptt+bptt_block; a++) bptt_history[a]=-1;
	//
	bptt_hidden=(neuron *)calloc((bptt+bptt_block+1)*layer1_size, sizeof(neuron));
	for (a=0; a<(bptt+bptt_block)*layer1_size; a++) {
	    bptt_hidden[a].ac=0;
	    bptt_hidden[a].er=0;
	}
	//
	bptt_syn0=(struct synapse *)calloc(layer0_size*layer1_size, sizeof(struct synapse));
	if (bptt_syn0==NULL) {
	    printf("Memory allocation failed\n");
	    exit(1);
	}
    }

    saveWeights();
    
    double df, dd;
    int i;
    
    df=0;
    dd=0;
    a=0;
    b=0;

    if (old_classes) {  	// old classes
        for (i=0; i<vocab_size; i++) b+=vocab[i].cn;
        for (i=0; i<vocab_size; i++) {
            df+=vocab[i].cn/(double)b;
            if (df>1) df=1;
            if (df>(a+1)/(double)class_size) {
    	        vocab[i].class_index=a;
    	        if (a<class_size-1) a++;
            } else {
    	        vocab[i].class_index=a;
            }
        }
    } else {			// new classes
        for (i=0; i<vocab_size; i++) b+=vocab[i].cn;
        for (i=0; i<vocab_size; i++) dd+=sqrt(vocab[i].cn/(double)b);
        for (i=0; i<vocab_size; i++) {
	    df+=sqrt(vocab[i].cn/(double)b)/dd;
            if (df>1) df=1;
            if (df>(a+1)/(double)class_size) {
    	        vocab[i].class_index=a;
    	        if (a<class_size-1) a++;
            } else {
    	        vocab[i].class_index=a;
            }
	}
    }
    
    //allocate auxiliary class variables (for faster search when normalizing probability at output layer)
    
    class_words=(int **)calloc(class_size, sizeof(int *));
    class_cn=(int *)calloc(class_size, sizeof(int));
    class_max_cn=(int *)calloc(class_size, sizeof(int));
    
    for (i=0; i<class_size; i++) {
	class_cn[i]=0;
	class_max_cn[i]=10;
	class_words[i]=(int *)calloc(class_max_cn[i], sizeof(int));
    }
    
    for (i=0; i<vocab_size; i++) {
		cl=vocab[i].class_index;
		class_words[cl][class_cn[cl]]=i;
		class_cn[cl]++;
		if (class_cn[cl]+2>=class_max_cn[cl]) {
			class_max_cn[cl]+=10;
			class_words[cl]=(int *)realloc(class_words[cl], class_max_cn[cl]*sizeof(int));
		}
    }
}

void CRnnLM::saveNet()       //will save the whole network structure                                                        
{
    FILE *fo;
    int a, b;
    char str[1000];
    float fl;
    
    sprintf(str, "%s.temp", rnnlm_file);

    fo=fopen(str, "wb");
    if (fo==NULL) {
        printf("Cannot create file %s\n", rnnlm_file);
        exit(1);
    }
    fprintf(fo, "version: %d\n", version);
    fprintf(fo, "file format: %d\n\n", filetype);

    fprintf(fo, "training data file: %s\n", train_file);
    fprintf(fo, "validation data file: %s\n\n", valid_file);

    fprintf(fo, "last probability of validation data: %f\n", llogp);
	fprintf(fo, "last last probability of validation data: %f\n", lllogp);//prnnlm
    fprintf(fo, "number of finished iterations: %d\n", iter);

    fprintf(fo, "current position in training data: %d\n", train_cur_pos);
    fprintf(fo, "current probability of training data: %f\n", logp);
    fprintf(fo, "save after processing # words: %d\n", anti_k);
    fprintf(fo, "# of training words: %d\n", train_words);

    fprintf(fo, "input layer size: %d\n", layer0_size);
    fprintf(fo, "hidden layer size: %d\n", layer1_size);
    fprintf(fo, "compression layer size: %d\n", layerc_size);
    fprintf(fo, "output layer size: %d\n", layer2_size);
    
    //prnnlm
    fprintf(fo, "latent variable size: %d\n", var_size);
	fprintf(fo, "latent variable hidden size: %d\n", varh_size);
    //end prnnlm

    fprintf(fo, "direct connections: %lld\n", direct_size);
    fprintf(fo, "direct order: %d\n", direct_order);
    
    fprintf(fo, "bptt: %d\n", bptt);
    fprintf(fo, "bptt block: %d\n", bptt_block);
    
    fprintf(fo, "vocabulary size: %d\n", vocab_size);
    fprintf(fo, "class size: %d\n", class_size);
    
    fprintf(fo, "old classes: %d\n", old_classes);
    fprintf(fo, "independent sentences mode: %d\n", independent);
    
    fprintf(fo, "starting learning rate: %f\n", starting_alpha);
    fprintf(fo, "current learning rate: %f\n", alpha);
    fprintf(fo, "learning rate decrease: %d\n", alpha_divide);
    fprintf(fo, "\n");

    fprintf(fo, "\nVocabulary:\n");
    for (a=0; a<vocab_size; a++) fprintf(fo, "%6d\t%10d\t%s\t%d\n", a, vocab[a].cn, vocab[a].word, vocab[a].class_index);

    
    if (filetype==TEXT) {
	fprintf(fo, "\nHidden layer activation:\n");
	for (a=0; a<layer1_size; a++) fprintf(fo, "%.4f\n", neu1[a].ac);
    }
    if (filetype==BINARY) {
    	for (a=0; a<layer1_size; a++) {
    	    fl=neu1[a].ac;
    	    fwrite(&fl, sizeof(fl), 1, fo);
    	}
    }
    //////////
    if (filetype==TEXT) {
	fprintf(fo, "\nWeights 0->1:\n");
	for (b=0; b<layer1_size; b++) {
    	    for (a=0; a<layer0_size; a++) {
        	fprintf(fo, "%.4f\n", syn0[a+b*layer0_size].weight);
    	    }
	}
    }
    if (filetype==BINARY) {
	for (b=0; b<layer1_size; b++) {
    	    for (a=0; a<layer0_size; a++) {
    		fl=syn0[a+b*layer0_size].weight;
    		fwrite(&fl, sizeof(fl), 1, fo);
    	    }
	}
    }
    /////////
    if (filetype==TEXT) {
		if (layerc_size>0) {
			fprintf(fo, "\n\nWeights 1->c:\n");
			for (b=0; b<layerc_size; b++) {
			for (a=0; a<layer1_size; a++) {
					fprintf(fo, "%.4f\n", syn1[a+b*layer1_size].weight);
				}
				}
			
				fprintf(fo, "\n\nWeights c->2:\n");
			for (b=0; b<layer2_size; b++) {
			for (a=0; a<layerc_size; a++) {
					fprintf(fo, "%.4f\n", sync[a+b*layerc_size].weight);
				}
				}
		}
		else
		{
			fprintf(fo, "\n\nWeights 1->2:\n");
			for (b=0; b<layer2_size; b++) {
			for (a=0; a<layer1_size; a++) {
					fprintf(fo, "%.4f\n", syn1[a+b*layer1_size].weight);
				}
				}
		}
    }
    if (filetype==BINARY) {
	if (layerc_size>0) {
	    for (b=0; b<layerc_size; b++) {
		for (a=0; a<layer1_size; a++) {
		    fl=syn1[a+b*layer1_size].weight;
    		    fwrite(&fl, sizeof(fl), 1, fo);
    		}
    	    }
    	
	    for (b=0; b<layer2_size; b++) {
		for (a=0; a<layerc_size; a++) {
    		    fl=sync[a+b*layerc_size].weight;
    		    fwrite(&fl, sizeof(fl), 1, fo);
    		}
    	    }
	}
	else
	{
	    for (b=0; b<layer2_size; b++) {
		for (a=0; a<layer1_size; a++) {
    		    fl=syn1[a+b*layer1_size].weight;
    		    fwrite(&fl, sizeof(fl), 1, fo);
    		}
    	    }
    	}
    }
    //prnnlm
    if (filetype==TEXT) {
        if (layerc_size>0) {
            fprintf(fo, "\n\nWeights c->v:\n");
            for (b=0; b<var_size; b++) {
                for (a=0; a<layerc_size; a++) {
                    fprintf(fo, "%.4f\n", syn2[a+b*layerc_size].weight);
                }
            }
        }
        else {
            fprintf(fo, "\n\nWeights 1->v:\n");
            for (b=0; b<var_size; b++) {
                for (a=0; a<layer1_size; a++) {
                    fprintf(fo, "%.4f\n", syn2[a+b*layer1_size].weight);
                }
            }
        }
       
        fprintf(fo, "\n\nWeights vv->vh:\n");
        for (b=0; b<var_size+varh_size; b++) {
            for (a=0; a<varh_size; a++) {
                fprintf(fo, "%.4f\n", syn3[a+b*varh_size].weight);
            }
        }
		
		fprintf(fo, "\n\nWeights vh->v:\n");
        for (b=0; b<varh_size; b++) {
            for (a=0; a<var_size; a++) {
                fprintf(fo, "%.4f\n", syn5[a+b*var_size].weight);
            }
        }

        fprintf(fo, "\n\nWeights v->2:\n");
        for (b=0; b<layer2_size; b++) {
            for (a=0; a<var_size; a++) {
                fprintf(fo, "%.4f\n", syn4[a+b*var_size].weight);
            }
        }
		
		fprintf(fo, "\n\nWeights vh->2:\n");
        for (b=0; b<layer2_size; b++) {
            for (a=0; a<varh_size; a++) {
                fprintf(fo, "%.4f\n", syn6[a+b*varh_size].weight);
            }
        }
		
		fprintf(fo, "\n\nWeights vh->h:\n");
        for (b=0; b<layer1_size; b++) {
            for (a=0; a<varh_size; a++) {
                fprintf(fo, "%.4f\n", syn7[a+b*varh_size].weight);
            }
        }
		
		fprintf(fo, "\n\nWeights v->1:\n");
        for (b=0; b<layer1_size; b++) {
            for (a=0; a<var_size; a++) {
                fprintf(fo, "%.4f\n", syn8[a+b*var_size].weight);
            }
        }
    }
    else
        printf("only text mode supported!");
    //end prnnlm
    ////////
    if (filetype==TEXT) {
	fprintf(fo, "\nDirect connections:\n");
	long long aa;
 	for (aa=0; aa<direct_size; aa++) {
    	    fprintf(fo, "%.2f\n", syn_d[aa]);
	}
    }
    if (filetype==BINARY) {
	long long aa;
	for (aa=0; aa<direct_size; aa++) {
    	    fl=syn_d[aa];
    	    fwrite(&fl, sizeof(fl), 1, fo);
    	    
    	    /*fl=syn_d[aa]*4*256;			//saving direct connections this way will save 50% disk space; several times more compression is doable by clustering
    	    if (fl>(1<<15)-1) fl=(1<<15)-1;
    	    if (fl<-(1<<15)) fl=-(1<<15);
    	    si=(signed short int)fl;
    	    fwrite(&si, 2, 1, fo);*/
	}
    }
    ////////    
    fclose(fo);
    //printf("end saveNet()!"); //prnnlm
    rename(str, rnnlm_file);
}

void CRnnLM::goToDelimiter(int delim, FILE *fi)
{
    int ch=0;

    while (ch!=delim) {
        ch=fgetc(fi);
        if (feof(fi)) {
            printf("Unexpected end of file\n");
            exit(1);
        }
    }
}

void CRnnLM::restoreNet()    //will read whole network structure
{
    FILE *fi;
    int a, b, ver;
    float fl;
    char str[MAX_STRING];
    double d;

    fi=fopen(rnnlm_file, "rb");
    if (fi==NULL) {
	printf("ERROR: model file '%s' not found!\n", rnnlm_file);
	exit(1);
    }

    goToDelimiter(':', fi);
    fscanf(fi, "%d", &ver);
    if ((ver==4) && (version==5)) /* we will solve this later.. */ ; else
    if (ver!=version) {
        printf("Unknown version of file %s\n", rnnlm_file);
        exit(1);
    }
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &filetype);
    //
    goToDelimiter(':', fi);
    if (train_file_set==0) {
	fscanf(fi, "%s", train_file);
    } else fscanf(fi, "%s", str);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%s", valid_file);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%lf", &llogp);
	//
    goToDelimiter(':', fi);
    fscanf(fi, "%lf", &lllogp);//prnnlm
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &iter);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &train_cur_pos);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%lf", &logp);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &anti_k);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &train_words);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &layer0_size);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &layer1_size);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &layerc_size);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &layer2_size);

    //prnnlm
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &var_size);
	goToDelimiter(':', fi);
    fscanf(fi, "%d", &varh_size);
    //end prnnlm
    
    //
    if (ver>5) {
	goToDelimiter(':', fi);
	fscanf(fi, "%lld", &direct_size);
    }
    //
    if (ver>6) {
	goToDelimiter(':', fi);
	fscanf(fi, "%d", &direct_order);
    }
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &bptt);
    //
    if (ver>4) {
	goToDelimiter(':', fi);
	fscanf(fi, "%d", &bptt_block);
    } else bptt_block=10;
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &vocab_size);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &class_size);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &old_classes);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &independent);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%lf", &d);
    starting_alpha=d;
    //
    goToDelimiter(':', fi);
    if (alpha_set==0) {
	fscanf(fi, "%lf", &d);
	alpha=d;
    } else fscanf(fi, "%lf", &d);
    //
    goToDelimiter(':', fi);
    fscanf(fi, "%d", &alpha_divide);
    //
    
    
    //read normal vocabulary
    if (vocab_max_size<vocab_size) {
	if (vocab!=NULL) free(vocab);
        vocab_max_size=vocab_size+1000;
        vocab=(struct vocab_word *)calloc(vocab_max_size, sizeof(struct vocab_word));    //initialize memory for vocabulary
    }
    //
    goToDelimiter(':', fi);
    for (a=0; a<vocab_size; a++) {
	//fscanf(fi, "%d%d%s%d", &b, &vocab[a].cn, vocab[a].word, &vocab[a].class_index);
	fscanf(fi, "%d%d", &b, &vocab[a].cn);
	readWord(vocab[a].word, fi);
	fscanf(fi, "%d", &vocab[a].class_index);
	//printf("%d  %d  %s  %d\n", b, vocab[a].cn, vocab[a].word, vocab[a].class_index);
    }
    //
    if (neu0==NULL) initNet();		//memory allocation here
    //
    
    
    if (filetype==TEXT) {
	goToDelimiter(':', fi);
	for (a=0; a<layer1_size; a++) {
	    fscanf(fi, "%lf", &d);
	    neu1[a].ac=d;
	}
    }
    if (filetype==BINARY) {
	fgetc(fi);
	for (a=0; a<layer1_size; a++) {
	    fread(&fl, sizeof(fl), 1, fi);
	    neu1[a].ac=fl;
	}
    }
    //
    if (filetype==TEXT) {
	goToDelimiter(':', fi);
	for (b=0; b<layer1_size; b++) {
    	    for (a=0; a<layer0_size; a++) {
		fscanf(fi, "%lf", &d);
		syn0[a+b*layer0_size].weight=d;
	    }
	}
    }
    if (filetype==BINARY) {
	for (b=0; b<layer1_size; b++) {
    	    for (a=0; a<layer0_size; a++) {
    		fread(&fl, sizeof(fl), 1, fi);
		syn0[a+b*layer0_size].weight=fl;
    	    }
	}
    }
    //
    if (filetype==TEXT) {
	goToDelimiter(':', fi);
	if (layerc_size==0) {	//no compress layer
	    for (b=0; b<layer2_size; b++) {
    		for (a=0; a<layer1_size; a++) {
		    fscanf(fi, "%lf", &d);
		    syn1[a+b*layer1_size].weight=d;
		}
    	    }
	}
	else
	{				//with compress layer
	    for (b=0; b<layerc_size; b++) {
    		for (a=0; a<layer1_size; a++) {
		    fscanf(fi, "%lf", &d);
		    syn1[a+b*layer1_size].weight=d;
		}
    	    }
    	
    	    goToDelimiter(':', fi);
    	
    	    for (b=0; b<layer2_size; b++) {
    		for (a=0; a<layerc_size; a++) {
		    fscanf(fi, "%lf", &d);
		    sync[a+b*layerc_size].weight=d;
		}
    	    }
	}
    }
    if (filetype==BINARY) {
	if (layerc_size==0) {	//no compress layer
	    for (b=0; b<layer2_size; b++) {
    		for (a=0; a<layer1_size; a++) {
    		    fread(&fl, sizeof(fl), 1, fi);
		    syn1[a+b*layer1_size].weight=fl;
    		}
    	    }
	}
	else
	{				//with compress layer
	    for (b=0; b<layerc_size; b++) {
    		for (a=0; a<layer1_size; a++) {
    		    fread(&fl, sizeof(fl), 1, fi);
		    syn1[a+b*layer1_size].weight=fl;
    		}
    	    }
    	
    	    for (b=0; b<layer2_size; b++) {
    		for (a=0; a<layerc_size; a++) {
    		    fread(&fl, sizeof(fl), 1, fi);
		    sync[a+b*layerc_size].weight=fl;
    		}    	    }
	}
    }
    //prnnlm
   if (filetype==TEXT) {
       goToDelimiter(':', fi);
        if (layerc_size==0) {
            for (b=0; b<var_size; b++) {
                for (a=0; a<layer1_size; a++) {
                    fscanf(fi, "%lf", &d);
                    syn2[a+b*layer1_size].weight=d;
                }
            }
        }
        else
        {
            for (b=0; b<var_size; b++) {
                for (a=0; a<layerc_size; a++) {
                    fscanf(fi, "%lf", &d);
                    syn2[a+b*layerc_size].weight=d;
                }
            }
        }

        /*goToDelimiter(':', fi);
        for (b=0; b<var_size; b++)
            for (a=0; a<var_size; a++) {
                fscanf(fi, "%lf", &d);
                syn3[a+b*var_size].weight=d;
            }*/ //old syn3
			
		goToDelimiter(':', fi);	
        for (b=0; b<var_size+varh_size; b++) {
            for (a=0; a<varh_size; a++) {
                fscanf(fi, "%lf", &d);
                syn3[a+b*varh_size].weight=d;
            }
        }
		
		goToDelimiter(':', fi);
        for (b=0; b<varh_size; b++) {
            for (a=0; a<var_size; a++) {
                fscanf(fi, "%lf", &d);
                syn5[a+b*var_size].weight=d;
            }
        }
  
        goToDelimiter(':', fi);
        for (b=0; b<layer2_size; b++)
            for (a=0; a<var_size; a++) {
                fscanf(fi, "%lf", &d);
                syn4[a+b*var_size].weight=d;
            }
			
		goToDelimiter(':', fi);
        for (b=0; b<layer2_size; b++) {
            for (a=0; a<varh_size; a++) {
				fscanf(fi, "%lf", &d);
                syn6[a+b*varh_size].weight=d;
            }
        }
		
		goToDelimiter(':', fi);
        for (b=0; b<layer1_size; b++) {
            for (a=0; a<varh_size; a++) {
				fscanf(fi, "%lf", &d);
                syn7[a+b*varh_size].weight=d;
            }
        }
		
		goToDelimiter(':', fi);
        for (b=0; b<layer1_size; b++) {
            for (a=0; a<var_size; a++) {
				fscanf(fi, "%lf", &d);
                syn8[a+b*var_size].weight=d;
            }
        }
    }
    else
        printf("only text mode supported!");
    //end prnnlm
    //
    if (filetype==TEXT) {
	goToDelimiter(':', fi);		//direct conenctions
	long long aa;
    	for (aa=0; aa<direct_size; aa++) {
	    fscanf(fi, "%lf", &d);
	    syn_d[aa]=d;
	}
    }
    //
    if (filetype==BINARY) {
	long long aa;
    	for (aa=0; aa<direct_size; aa++) {
    	    fread(&fl, sizeof(fl), 1, fi);
	    syn_d[aa]=fl;
	    
	    /*fread(&si, 2, 1, fi);
	    fl=si/(float)(4*256);
	    syn_d[aa]=fl;*/
    	}
    }
    //
    
    saveWeights();

    fclose(fi);
}

void CRnnLM::netFlush()   //cleans all activations and error vectors
{
    int a;

    for (a=0; a<layer0_size-layer1_size; a++) {
        neu0[a].ac=0;
        neu0[a].er=0;
    }

    for (a=layer0_size-layer1_size; a<layer0_size; a++) {   //last hidden layer is initialized to vector of 0.1 values to prevent unstability
        neu0[a].ac=0.1;
        neu0[a].er=0;
    }

    for (a=0; a<layer1_size; a++) {
        neu1[a].ac=0;
        neu1[a].er=0;
    }
    
    for (a=0; a<layerc_size; a++) {
        neuc[a].ac=0;
        neuc[a].er=0;
    }
    
    for (a=0; a<layer2_size; a++) {
        neu2[a].ac=0;
        neu2[a].er=0;
    }

    //prnnlm
    for (a=0; a<var_size; a++) {
        neuv[a].ac=0;
        neuv[a].er=0;
        neuvv[a].ac=0;
        neuvv[a].er=0;
    }
	for (a=var_size; a<var_size+varh_size; a++) {
		neuvv[a].ac=0;
		neuvv[a].er=0;
	}
	for (a=0; a<varh_size; a++) {
        neuvh[a].ac=0;
        neuvh[a].er=0;
    }
    //end prnnlm
}

void CRnnLM::netReset()   //cleans hidden layer activation + bptt history
{
    int a, b;

    for (a=0; a<layer1_size; a++) {
        neu1[a].ac=1.0;
    }

    //prnnlm
    //for (a=0; a<var_size; a++) {
    //    neuv[a].ac=1.0;
    //}
	for (a=0; a<varh_size; a++) {
		neuvh[a].ac=1.0;
	}
    //end prnnlm

    copyHiddenLayerToInput();

    if (bptt>0) {
        for (a=1; a<bptt+bptt_block; a++) bptt_history[a]=0;
        for (a=bptt+bptt_block-1; a>1; a--) for (b=0; b<layer1_size; b++) {
            bptt_hidden[a*layer1_size+b].ac=0;
            bptt_hidden[a*layer1_size+b].er=0;
        }
    }

    for (a=0; a<MAX_NGRAM_ORDER; a++) history[a]=0;
}

void CRnnLM::matrixXvector(struct neuron *dest, struct neuron *srcvec, struct synapse *srcmatrix, int matrix_width, int from, int to, int from2, int to2, int type)
{
    int a, b;
    real val1, val2, val3, val4;
    real val5, val6, val7, val8;
    
    if (type==0) {		//ac mod
	for (b=0; b<(to-from)/8; b++) {
	    val1=0;
	    val2=0;
	    val3=0;
	    val4=0;
	    
	    val5=0;
	    val6=0;
	    val7=0;
	    val8=0;
	    
	    for (a=from2; a<to2; a++) {
    		val1 += srcvec[a].ac * srcmatrix[a+(b*8+from+0)*matrix_width].weight;
    		val2 += srcvec[a].ac * srcmatrix[a+(b*8+from+1)*matrix_width].weight;
    		val3 += srcvec[a].ac * srcmatrix[a+(b*8+from+2)*matrix_width].weight;
    		val4 += srcvec[a].ac * srcmatrix[a+(b*8+from+3)*matrix_width].weight;
    		
    		val5 += srcvec[a].ac * srcmatrix[a+(b*8+from+4)*matrix_width].weight;
    		val6 += srcvec[a].ac * srcmatrix[a+(b*8+from+5)*matrix_width].weight;
    		val7 += srcvec[a].ac * srcmatrix[a+(b*8+from+6)*matrix_width].weight;
    		val8 += srcvec[a].ac * srcmatrix[a+(b*8+from+7)*matrix_width].weight;
    	    }
    	    dest[b*8+from+0].ac += val1;
    	    dest[b*8+from+1].ac += val2;
    	    dest[b*8+from+2].ac += val3;
    	    dest[b*8+from+3].ac += val4;
    	    
    	    dest[b*8+from+4].ac += val5;
    	    dest[b*8+from+5].ac += val6;
    	    dest[b*8+from+6].ac += val7;
    	    dest[b*8+from+7].ac += val8;
	}
    
	for (b=b*8; b<to-from; b++) {
	    for (a=from2; a<to2; a++) {
    		dest[b+from].ac += srcvec[a].ac * srcmatrix[a+(b+from)*matrix_width].weight;
    	    }
    	}
    }
    else {		//er mod
    	for (a=0; a<(to2-from2)/8; a++) {
	    val1=0;
	    val2=0;
	    val3=0;
	    val4=0;
	    
	    val5=0;
	    val6=0;
	    val7=0;
	    val8=0;
	    
	    for (b=from; b<to; b++) {
    	        val1 += srcvec[b].er * srcmatrix[a*8+from2+0+b*matrix_width].weight;
    	        val2 += srcvec[b].er * srcmatrix[a*8+from2+1+b*matrix_width].weight;
    	        val3 += srcvec[b].er * srcmatrix[a*8+from2+2+b*matrix_width].weight;
    	        val4 += srcvec[b].er * srcmatrix[a*8+from2+3+b*matrix_width].weight;
    	        
    	        val5 += srcvec[b].er * srcmatrix[a*8+from2+4+b*matrix_width].weight;
    	        val6 += srcvec[b].er * srcmatrix[a*8+from2+5+b*matrix_width].weight;
    	        val7 += srcvec[b].er * srcmatrix[a*8+from2+6+b*matrix_width].weight;
    	        val8 += srcvec[b].er * srcmatrix[a*8+from2+7+b*matrix_width].weight;
    	    }
    	    dest[a*8+from2+0].er += val1;
    	    dest[a*8+from2+1].er += val2;
    	    dest[a*8+from2+2].er += val3;
    	    dest[a*8+from2+3].er += val4;
    	    
    	    dest[a*8+from2+4].er += val5;
    	    dest[a*8+from2+5].er += val6;
    	    dest[a*8+from2+6].er += val7;
    	    dest[a*8+from2+7].er += val8;
	}
	
	for (a=a*8; a<to2-from2; a++) {
	    for (b=from; b<to; b++) {
    		dest[a+from2].er += srcvec[b].er * srcmatrix[a+from2+b*matrix_width].weight;
    	    }
    	}
    	
    	if (gradient_cutoff>0)
    	for (a=from2; a<to2; a++) {
    	    if (dest[a].er>gradient_cutoff) dest[a].er=gradient_cutoff;
    	    if (dest[a].er<-gradient_cutoff) dest[a].er=-gradient_cutoff;
    	}
    }
    
    //this is normal implementation (about 3x slower):
    
    /*if (type==0) {		//ac mod
	for (b=from; b<to; b++) {
	    for (a=from2; a<to2; a++) {
    		dest[b].ac += srcvec[a].ac * srcmatrix[a+b*matrix_width].weight;
    	    }
	}
    }
    else 		//er mod
    if (type==1) {
	for (a=from2; a<to2; a++) {
	    for (b=from; b<to; b++) {
    		dest[a].er += srcvec[b].er * srcmatrix[a+b*matrix_width].weight;
    	    }
    	}
    }*/
}

void CRnnLM::computeNet(int last_word, int word)
{
    int a, b, c;
    real val;
    double sum;   //sum is used for normalization: it's better to have larger precision as many numbers are summed together here
    
    if (last_word!=-1) neu0[last_word].ac=1;

    //propagate 0->1
    for (a=0; a<layer1_size; a++) neu1[a].ac=0;
    for (a=0; a<layerc_size; a++) neuc[a].ac=0;
    
    matrixXvector(neu1, neu0, syn0, layer0_size, 0, layer1_size, layer0_size-layer1_size, layer0_size, 0);
	
    for (b=0; b<layer1_size; b++) {
        a=last_word;
        if (a!=-1) neu1[b].ac += neu0[a].ac * syn0[a+b*layer0_size].weight;
    }

    
    //prnnlm: update var
    if (var_update) {
        //if (last_var!=-1) {
          //  for (a=0; a<var_size; a++)
            //    neuvv[a].ac=0;
            //neuvv[last_var].ac=1;
        //}
        //else //copy neuv to neuvv
        //{
            for (a=0; a<var_size; a++) {
                neuvv[a].ac=neuv[a].ac;
				neuv[a].ac=0;
			}
        //}
        if (last_var!=-1) {
           for (a=0; a<var_size; a++)
              neuvv[a].ac=0;
           neuvv[last_var].ac=1;
        }
		for(a=0;a<varh_size;a++) neuvh[a].ac=0;

        
		matrixXvector(neuvh, neuvv, syn3, var_size+varh_size, 0, varh_size, var_size, var_size+varh_size, 0);
		for (b=0; b<varh_size; b++) {
			a=last_var;
			if (a!=-1) neuvh[b].ac += neuvv[a].ac * syn3[a+b*(var_size+varh_size)].weight;
		}
		//activation varh --sigmoid on variable hidden layer
		for (a=0; a<varh_size; a++) {
            if (neuvh[a].ac>50) neuvh[a].ac=50;
            if (neuvh[a].ac<-50) neuvh[a].ac=-50;
            val=-neuvh[a].ac;
            neuvh[a].ac=1/(1+fasterexp(val));
        }
		
		matrixXvector(neuv, neuvh, syn5, varh_size, 0, var_size, 0, varh_size, 0);
		matrixXvector(neu1, neuvh, syn7, varh_size, 0, layer1_size, 0, varh_size, 0);
		//matrixXvector(neuv, neu1, syn2, layer1_size, 0, var_size, 0, layer1_size, 0);
        //activation var --sigmoid on variables
		if(use_gold_pos) {
			for (a=0; a<var_size; a++) {
				neuvb[a].ac=0;
				neuvb[a].er=0;
			}
			if (var!=-1) neuvb[var].ac=1;
		} else {
			for (a=0; a<var_size; a++) {
				neuvb[a].ac=neuv[a].ac;
				neuvb[a].er=neuv[a].er;
			}
			for (a=0; a<var_size; a++) {
				if (neuvb[a].ac>50) neuvb[a].ac=50;
				if (neuvb[a].ac<-50) neuvb[a].ac=-50;
				val=-neuvb[a].ac;
				neuvb[a].ac=1/(1+fasterexp(val));
			}
		}
        //activation var  --softmax on variables
        sum=0;
        real maxAc=-FLT_MAX;
        for (a=0; a<var_size; a++)
            if (neuv[a].ac>maxAc) maxAc=neuv[a].ac;
        for (a=0; a<var_size; a++)
            sum+=fasterexp(neuv[a].ac-maxAc);
        for (a=0; a<var_size; a++)
            neuv[a].ac=fasterexp(neuv[a].ac-maxAc)/sum;
		//output latent variable
		/*a=0;b=0;
		val = -999.0;
		for (a=0;a<var_size;a++) {
			if (neuv[a].ac>val) {
				val=neuv[a].ac;
				b=a;
			}
		}
		FILE *fl;
		fl=fopen("latent","a");
		fprintf(fl,"%d %d %lf", var, b, val);
		//for (a=0;a<var_size;a++) {
		//	fprintf(fl, " %lf", neuv[a].ac);
		//}
		fprintf(fl, "\n");
		fclose(fl);*/
    }
    //end prnnlm   

	//activate 1      --sigmoid
    for (a=0; a<layer1_size; a++) {
	if (neu1[a].ac>50) neu1[a].ac=50;  //for numerical stability
        if (neu1[a].ac<-50) neu1[a].ac=-50;  //for numerical stability
        val=-neu1[a].ac;
        neu1[a].ac=1/(1+fasterexp(val));
    }
    
    if (layerc_size>0) {
		matrixXvector(neuc, neu1, syn1, layer1_size, 0, layerc_size, 0, layer1_size, 0);
		//activate compression      --sigmoid
		for (a=0; a<layerc_size; a++) {
			if (neuc[a].ac>50) neuc[a].ac=50;  //for numerical stability
				if (neuc[a].ac<-50) neuc[a].ac=-50;  //for numerical stability
				val=-neuc[a].ac;
				neuc[a].ac=1/(1+fasterexp(val));
		}
    }
	
    //1->2 class
    for (b=vocab_size; b<layer2_size; b++) neu2[b].ac=0;
    
    if (layerc_size>0) {
		matrixXvector(neu2, neuc, sync, layerc_size, vocab_size, layer2_size, 0, layerc_size, 0);
    }
    else
    {
		matrixXvector(neu2, neu1, syn1, layer1_size, vocab_size, layer2_size, 0, layer1_size, 0);
    }

    matrixXvector(neu2, neuvb, syn4, var_size, vocab_size, layer2_size, 0, var_size, 0); //prnnlm
	//matrixXvector(neu2, neuvh, syn6, varh_size, vocab_size, layer2_size, 0, varh_size, 0); //prnnlm
    //apply direct connections to classes
    if (direct_size>0) {
		unsigned long long hash[MAX_NGRAM_ORDER];	//this will hold pointers to syn_d that contains hash parameters
		
		for (a=0; a<direct_order; a++) hash[a]=0;
		
		for (a=0; a<direct_order; a++) {
			b=0;
			if (a>0) if (history[a-1]==-1) break;	//if OOV was in history, do not use this N-gram feature and higher orders
			hash[a]=PRIMES[0]*PRIMES[1];
					
			for (b=1; b<=a; b++) hash[a]+=PRIMES[(a*PRIMES[b]+b)%PRIMES_SIZE]*(unsigned long long)(history[b-1]+1);	//update hash value based on words from the history
			hash[a]=hash[a]%(direct_size/2);		//make sure that starting hash index is in the first half of syn_d (second part is reserved for history->words features)
		}
	
		for (a=vocab_size; a<layer2_size; a++) {
			for (b=0; b<direct_order; b++) if (hash[b]) {
			neu2[a].ac+=syn_d[hash[b]];		//apply current parameter and move to the next one
			hash[b]++;
			} else break;
		}
    }

    //activation 2   --softmax on classes
    // 20130425 - this is now a 'safe' softmax

    sum=0;
    real maxAc=-FLT_MAX;
    for (a=vocab_size; a<layer2_size; a++)
        if (neu2[a].ac>maxAc) maxAc=neu2[a].ac; //this prevents the need to check for overflow
    for (a=vocab_size; a<layer2_size; a++)
        sum+=fasterexp(neu2[a].ac-maxAc);
    for (a=vocab_size; a<layer2_size; a++)
        neu2[a].ac=fasterexp(neu2[a].ac-maxAc)/sum; 
 
    if (gen>0) return;	//if we generate words, we don't know what current word is -> only classes are estimated and word is selected in testGen()

    
    //1->2 word
    
    if (word!=-1) {
        for (c=0; c<class_cn[vocab[word].class_index]; c++) neu2[class_words[vocab[word].class_index][c]].ac=0;
        if (layerc_size>0) {
			matrixXvector(neu2, neuc, sync, layerc_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, layerc_size, 0);
		}
		else
		{
			matrixXvector(neu2, neu1, syn1, layer1_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, layer1_size, 0);
		}
    }
   
    matrixXvector(neu2, neuvb, syn4, var_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, var_size, 0); //prnnlm
	//matrixXvector(neu2, neuvh, syn6, varh_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, varh_size, 0); //prnnlm
    //apply direct connections to words
    if (word!=-1) if (direct_size>0) {
		unsigned long long hash[MAX_NGRAM_ORDER];
			
		for (a=0; a<direct_order; a++) hash[a]=0;
		
		for (a=0; a<direct_order; a++) {
			b=0;
			if (a>0) if (history[a-1]==-1) break;
			hash[a]=PRIMES[0]*PRIMES[1]*(unsigned long long)(vocab[word].class_index+1);
					
			for (b=1; b<=a; b++) hash[a]+=PRIMES[(a*PRIMES[b]+b)%PRIMES_SIZE]*(unsigned long long)(history[b-1]+1);
			hash[a]=(hash[a]%(direct_size/2))+(direct_size)/2;
		}
		
		for (c=0; c<class_cn[vocab[word].class_index]; c++) {
			a=class_words[vocab[word].class_index][c];
			
			for (b=0; b<direct_order; b++) if (hash[b]) {
			neu2[a].ac+=syn_d[hash[b]];
			hash[b]++;
			hash[b]=hash[b]%direct_size;
			} else break;
		}
    }

    //activation 2   --softmax on words
    // 130425 - this is now a 'safe' softmax
    sum=0;
    if (word!=-1) { 
        maxAc=-FLT_MAX;
        for (c=0; c<class_cn[vocab[word].class_index]; c++) {
            a=class_words[vocab[word].class_index][c];
            if (neu2[a].ac>maxAc) maxAc=neu2[a].ac;
        }
        for (c=0; c<class_cn[vocab[word].class_index]; c++) {
            a=class_words[vocab[word].class_index][c];
            sum+=fasterexp(neu2[a].ac-maxAc);
        }
        for (c=0; c<class_cn[vocab[word].class_index]; c++) {
            a=class_words[vocab[word].class_index][c];
            neu2[a].ac=fasterexp(neu2[a].ac-maxAc)/sum; //this prevents the need to check for overflow
        }
    }
}

void CRnnLM::learnNet(int last_word, int word)
{
    int a, b, c, t, step;
    real beta2, beta3;

    beta2=beta*alpha;
    beta3=beta2*1;	//beta3 can be possibly larger than beta2, as that is useful on small datasets (if the final model is to be interpolated wich backoff model) - todo in the future

    if (word==-1) return;

    //compute error vectors
    for (c=0; c<class_cn[vocab[word].class_index]; c++) {
	a=class_words[vocab[word].class_index][c];
        neu2[a].er=(0-neu2[a].ac);
    }
    neu2[word].er=(1-neu2[word].ac);	//word part

    //flush error
    for (a=0; a<layer1_size; a++) neu1[a].er=0;
    for (a=0; a<layerc_size; a++) neuc[a].er=0;

    for (a=vocab_size; a<layer2_size; a++) {
        neu2[a].er=(0-neu2[a].ac);
    }
    neu2[vocab[word].class_index+vocab_size].er=(1-neu2[vocab[word].class_index+vocab_size].ac);	//class part
    
    //
    if (direct_size>0) {	//learn direct connections between words
	if (word!=-1) {
	    unsigned long long hash[MAX_NGRAM_ORDER];
	    
	    for (a=0; a<direct_order; a++) hash[a]=0;
	
	    for (a=0; a<direct_order; a++) {
		b=0;
		if (a>0) if (history[a-1]==-1) break;
		hash[a]=PRIMES[0]*PRIMES[1]*(unsigned long long)(vocab[word].class_index+1);
				
	        for (b=1; b<=a; b++) hash[a]+=PRIMES[(a*PRIMES[b]+b)%PRIMES_SIZE]*(unsigned long long)(history[b-1]+1);
		hash[a]=(hash[a]%(direct_size/2))+(direct_size)/2;
	    }
	
	    for (c=0; c<class_cn[vocab[word].class_index]; c++) {
		a=class_words[vocab[word].class_index][c];
	    
		for (b=0; b<direct_order; b++) if (hash[b]) {
		    syn_d[hash[b]]+=alpha*neu2[a].er - syn_d[hash[b]]*beta3;
		    hash[b]++;
		    hash[b]=hash[b]%direct_size;
		} else break;
	    }
	}
    }
    //
    //learn direct connections to classes
    if (direct_size>0) {	//learn direct connections between words and classes
	unsigned long long hash[MAX_NGRAM_ORDER];
	
	for (a=0; a<direct_order; a++) hash[a]=0;
	
	for (a=0; a<direct_order; a++) {
	    b=0;
	    if (a>0) if (history[a-1]==-1) break;
	    hash[a]=PRIMES[0]*PRIMES[1];
	    	    
	    for (b=1; b<=a; b++) hash[a]+=PRIMES[(a*PRIMES[b]+b)%PRIMES_SIZE]*(unsigned long long)(history[b-1]+1);
	    hash[a]=hash[a]%(direct_size/2);
	}
	
	for (a=vocab_size; a<layer2_size; a++) {
	    for (b=0; b<direct_order; b++) if (hash[b]) {
		syn_d[hash[b]]+=alpha*neu2[a].er - syn_d[hash[b]]*beta3;
		hash[b]++;
	    } else break;
	}
    }
    //
    
    
    if (layerc_size>0) {
		matrixXvector(neuc, neu2, sync, layerc_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, layerc_size, 1);
		
		t=class_words[vocab[word].class_index][0]*layerc_size;
		for (c=0; c<class_cn[vocab[word].class_index]; c++) {
			b=class_words[vocab[word].class_index][c];
			if ((counter%10)==0)	//regularization is done every 10. step
			for (a=0; a<layerc_size; a++) sync[a+t].weight+=alpha*neu2[b].er*neuc[a].ac - sync[a+t].weight*beta2;
			else
			for (a=0; a<layerc_size; a++) sync[a+t].weight+=alpha*neu2[b].er*neuc[a].ac;
			t+=layerc_size;
		}
		//
		matrixXvector(neuc, neu2, sync, layerc_size, vocab_size, layer2_size, 0, layerc_size, 1);		//propagates errors 2->c for classes
		
		c=vocab_size*layerc_size;
		for (b=vocab_size; b<layer2_size; b++) {
			if ((counter%10)==0) {	//regularization is done every 10. step
			for (a=0; a<layerc_size; a++) sync[a+c].weight+=alpha*neu2[b].er*neuc[a].ac - sync[a+c].weight*beta2;	//weight c->2 update
			}
			else {
				for (a=0; a<layerc_size; a++) sync[a+c].weight+=alpha*neu2[b].er*neuc[a].ac;	//weight c->2 update
			}
			c+=layerc_size;
		}
		
		for (a=0; a<layerc_size; a++) neuc[a].er=neuc[a].er*neuc[a].ac*(1-neuc[a].ac);    //error derivation at compression layer

		////
		
		matrixXvector(neu1, neuc, syn1, layer1_size, 0, layerc_size, 0, layer1_size, 1);		//propagates errors c->1
		
		for (b=0; b<layerc_size; b++) {
			for (a=0; a<layer1_size; a++) syn1[a+b*layer1_size].weight+=alpha*neuc[b].er*neu1[a].ac;	//weight 1->c update
		}
    }
    else
    {
    	matrixXvector(neu1, neu2, syn1, layer1_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, layer1_size, 1);
    	
    	t=class_words[vocab[word].class_index][0]*layer1_size;
		for (c=0; c<class_cn[vocab[word].class_index]; c++) {
			b=class_words[vocab[word].class_index][c];
			if ((counter%10)==0)	//regularization is done every 10. step
			for (a=0; a<layer1_size; a++) syn1[a+t].weight+=alpha*neu2[b].er*neu1[a].ac - syn1[a+t].weight*beta2;
			else
			for (a=0; a<layer1_size; a++) syn1[a+t].weight+=alpha*neu2[b].er*neu1[a].ac;
			t+=layer1_size;
		}
		//
		matrixXvector(neu1, neu2, syn1, layer1_size, vocab_size, layer2_size, 0, layer1_size, 1);		//propagates errors 2->1 for classes
	
		c=vocab_size*layer1_size;
		for (b=vocab_size; b<layer2_size; b++) {
			if ((counter%10)==0) {	//regularization is done every 10. step
			for (a=0; a<layer1_size; a++) syn1[a+c].weight+=alpha*neu2[b].er*neu1[a].ac - syn1[a+c].weight*beta2;	//weight 1->2 update
			}
			else {
				for (a=0; a<layer1_size; a++) syn1[a+c].weight+=alpha*neu2[b].er*neu1[a].ac;	//weight 1->2 update
			}
			c+=layer1_size;
		}
    }
   
    //prnnlm
    if(var_update) {
        
		//flush error
		for (a=0; a<var_size+varh_size; a++) neuvv[a].er=0;
		for (a=0; a<varh_size; a++) neuvh[a].er=0;
		for (a=0; a<var_size; a++) {
            neuv[a].er=0;
        }
        //if(last_var!=-1);
            //neuvv[last_var].er=(1-neuvv[last_var].ac);
    }
    if (var!=-1) {
		//error: 2-->v
        //matrixXvector(neuv, neu2, syn4, var_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, var_size, 1);
		//matrixXvector(neuv, neu2, syn4, var_size, vocab_size, layer2_size, 0, var_size, 1);
		//matrixXvector(neuvh, neu2, syn6, varh_size, class_words[vocab[word].class_index][0], class_words[vocab[word].class_index][0]+class_cn[vocab[word].class_index], 0, varh_size, 1);
		// error vectors
        for (a=0; a<var_size; a++) {
            neuv[a].er=(0-neuv[a].ac);
        }
        if (var!=-1)
            neuv[var].er=(1-neuv[var].ac);
		
		//for (a=0; a<var_size; a++) neuv[a].er=neuv[a].er*neuv[a].ac*(1-neuv[a].ac);    //error derivation at var layer
		
        /*t=class_words[vocab[word].class_index][0]*var_size;
        for (c=0; c<class_cn[vocab[word].class_index]; c++) {
            b=class_words[vocab[word].class_index][c];
            if ((counter%10)==0){        //regularization is done every 10. step
				for (a=0; a<var_size; a++)
                syn4[a+t].weight+=alpha*neu2[b].er*neuvb[a].ac - syn4[a+t].weight*beta2;
			}
            else 
				for (a=0; a<var_size; a++)
					syn4[a+t].weight+=alpha*neu2[b].er*neuvb[a].ac;
            t+=var_size;
        }

        c=vocab_size*var_size;
        for (b=vocab_size; b<layer2_size; b++) {
            if ((counter%10)==0) {     //regularization is done every 10. step
				for (a=0; a<var_size; a++)
					syn4[a+c].weight+=alpha*neu2[b].er*neuvb[a].ac - syn4[a+c].weight*beta2;
			}
            else
				for (a=0; a<var_size; a++)
					syn4[a+c].weight+=alpha*neu2[b].er*neuvb[a].ac;
            c+=var_size;
        }*/
		
		/*t=class_words[vocab[word].class_index][0]*varh_size;
        for (c=0; c<class_cn[vocab[word].class_index]; c++) {
            b=class_words[vocab[word].class_index][c];
            if ((counter%10)==0){        //regularization is done every 10. step
				for (a=0; a<varh_size; a++)
					syn6[a+t].weight+=alpha*neu2[b].er*neuvh[a].ac - syn6[a+t].weight*beta2;
			}
            else 
				for (a=0; a<varh_size; a++)
					syn6[a+t].weight+=alpha*neu2[b].er*neuvh[a].ac;
            t+=varh_size;
        }*/
/*
        c=vocab_size*varh_size;
        for (b=vocab_size; b<layer2_size; b++) {
            if ((counter%10)==0) {     //regularization is done every 10. step
				for (a=0; a<varh_size; a++)
					syn6[a+c].weight+=alpha*neu2[b].er*neuvh[a].ac - syn6[a+c].weight*beta2;
			}
            else
				for (a=0; a<varh_size; a++)
					syn6[a+c].weight+=alpha*neu2[b].er*neuvh[a].ac;
            c+=varh_size;
        }*/
    }
    if(var_update){
        // update synapse
        if (var!=-1) {
            //matrixXvector(neu1, neuv, syn2, layer1_size, 0, var_size, 0, layer1_size, 1);
			//v-->vh
			matrixXvector(neuvh, neuv, syn5, varh_size, 0, var_size, 0, varh_size, 1);
			
			//update syn5
			t=0;
			for (b=0; b<var_size; b++) {                
				if ((counter%10)==0)
					for (a=0; a<varh_size; a++) syn5[a+t].weight+=alpha*neuv[b].er*neuvh[a].ac - syn5[a+t].weight*beta2;
				else
					for (a=0; a<varh_size; a++) syn5[a+t].weight+=alpha*neuv[b].er*neuvh[a].ac;
				t+=varh_size;
            }
			
			//update syn7
			for (a=0; a<layer1_size; a++) {neu1b[a].ac=neu1[a].ac; neu1b[a].er=neu1[a].er;}
			for (a=0; a<layer1_size; a++) neu1b[a].er=neu1b[a].er*neu1b[a].ac*(1-neu1b[a].ac);    //error derivation at layer 1
			//matrixXvector(neuvh, neu1, syn7, varh_size, 0, layer1_size, 0, varh_size, 1);
			t=0;
			for (b=0; b<layer1_size; b++) {                
				if ((counter%10)==0)
					for (a=0; a<varh_size; a++) syn7[a+t].weight+=alpha*neu1b[b].er*neuvh[a].ac - syn7[a+t].weight*beta2;
				else
					for (a=0; a<varh_size; a++) syn7[a+t].weight+=alpha*neu1b[b].er*neuvh[a].ac;
				t+=varh_size;
            }
			
			for (a=0; a<varh_size; a++) neuvh[a].er=neuvh[a].er*neuvh[a].ac*(1-neuvh[a].ac);    //error derivation at var hidden layer
            matrixXvector(neuvv, neuvh, syn3, var_size+varh_size, 0, varh_size, 0, var_size+varh_size, 1);
			//update syn2
            /*t=0;
            for (b=0; b<var_size; b++) {
                if (layerc_size>0) {
                    if ((counter%10)==0)
                        for (a=0; a<layerc_size; a++) syn2[a+t].weight+=alpha*neuv[b].er*neuc[a].ac - syn2[a+t].weight*beta2;
                    else
                        for (a=0; a<layerc_size; a++) syn2[a+t].weight+=alpha*neuv[b].er*neuc[a].ac;
                    t+=layerc_size;
                }
                else {
                    if ((counter%10)==0)
                        for (a=0; a<layer1_size; a++) syn2[a+t].weight+=alpha*neuv[b].er*neu1[a].ac - syn2[a+t].weight*beta2;
                    else
                        for (a=0; a<layer1_size; a++)
                            syn2[a+t].weight+=alpha*neuv[b].er*neu1[a].ac;
                   t+=layer1_size;
                }
            }*/
			//update syn3
			a=last_var;
			if (a!=-1) {
				if ((counter%10)==0)
                    for (b=0; b<varh_size; b++) syn3[a+b*(var_size+varh_size)].weight+=alpha*neuvh[b].er*neuvv[a].ac - syn3[a+b*(var_size+varh_size)].weight*beta2;
                else
                    for (b=0; b<varh_size; b++) syn3[a+b*(var_size+varh_size)].weight+=alpha*neuvh[b].er*neuvv[a].ac;
			}

			//neuh->neuvv(hidden part)
            t=0;
            for (b=0; b<varh_size; b++) {
                if ((counter%10)==0)
                    for (a=varh_size; a<varh_size+var_size; a++) syn3[a+t].weight+=alpha*neuvh[b].er*neuvv[a].ac - syn3[a+t].weight*beta2;
                else
                    for (a=varh_size; a<varh_size+var_size; a++) syn3[a+t].weight+=alpha*neuvh[b].er*neuvv[a].ac;
                t+=var_size+varh_size;
            }
        }
    }
    //end prnnlm

    //
    
    ///////////////

    if (bptt<=1) {		//bptt==1 -> normal BP
		for (a=0; a<layer1_size; a++) neu1[a].er=neu1[a].er*neu1[a].ac*(1-neu1[a].ac);    //error derivation at layer 1

		//weight update 1->0
		a=last_word;
		if (a!=-1) {
			if ((counter%10)==0)
			for (b=0; b<layer1_size; b++) syn0[a+b*layer0_size].weight+=alpha*neu1[b].er*neu0[a].ac - syn0[a+b*layer0_size].weight*beta2;
			else
			for (b=0; b<layer1_size; b++) syn0[a+b*layer0_size].weight+=alpha*neu1[b].er*neu0[a].ac;
		}

		if ((counter%10)==0) {
			for (b=0; b<layer1_size; b++) for (a=layer0_size-layer1_size; a<layer0_size; a++) syn0[a+b*layer0_size].weight+=alpha*neu1[b].er*neu0[a].ac - syn0[a+b*layer0_size].weight*beta2;
		}
		else {
			for (b=0; b<layer1_size; b++) for (a=layer0_size-layer1_size; a<layer0_size; a++) syn0[a+b*layer0_size].weight+=alpha*neu1[b].er*neu0[a].ac;
		}
    }
    else		//BPTT
    {
		for (b=0; b<layer1_size; b++) bptt_hidden[b].ac=neu1[b].ac;
		for (b=0; b<layer1_size; b++) bptt_hidden[b].er=neu1[b].er;
		
		if (((counter%bptt_block)==0) || (independent && (word==0))) {
			for (step=0; step<bptt+bptt_block-2; step++) {
			for (a=0; a<layer1_size; a++) neu1[a].er=neu1[a].er*neu1[a].ac*(1-neu1[a].ac);    //error derivation at layer 1

			//weight update 1->0
			a=bptt_history[step];
			if (a!=-1)
			for (b=0; b<layer1_size; b++) {
					bptt_syn0[a+b*layer0_size].weight+=alpha*neu1[b].er;//*neu0[a].ac; --should be always set to 1
			}
			
			for (a=layer0_size-layer1_size; a<layer0_size; a++) neu0[a].er=0;
			
			matrixXvector(neu0, neu1, syn0, layer0_size, 0, layer1_size, layer0_size-layer1_size, layer0_size, 1);		//propagates errors 1->0
			for (b=0; b<layer1_size; b++) for (a=layer0_size-layer1_size; a<layer0_size; a++) {
				//neu0[a].er += neu1[b].er * syn0[a+b*layer0_size].weight;
					bptt_syn0[a+b*layer0_size].weight+=alpha*neu1[b].er*neu0[a].ac;
			}
			
			for (a=0; a<layer1_size; a++) {		//propagate error from time T-n to T-n-1
					neu1[a].er=neu0[a+layer0_size-layer1_size].er + bptt_hidden[(step+1)*layer1_size+a].er;
			}
			
			if (step<bptt+bptt_block-3)
			for (a=0; a<layer1_size; a++) {
				neu1[a].ac=bptt_hidden[(step+1)*layer1_size+a].ac;
				neu0[a+layer0_size-layer1_size].ac=bptt_hidden[(step+2)*layer1_size+a].ac;
			}
			}
			
			for (a=0; a<(bptt+bptt_block)*layer1_size; a++) {
				bptt_hidden[a].er=0;
			}
		
		
			for (b=0; b<layer1_size; b++) neu1[b].ac=bptt_hidden[b].ac;		//restore hidden layer after bptt
			
		
			//
			for (b=0; b<layer1_size; b++) {		//copy temporary syn0
			if ((counter%10)==0) {
				for (a=layer0_size-layer1_size; a<layer0_size; a++) {
						syn0[a+b*layer0_size].weight+=bptt_syn0[a+b*layer0_size].weight - syn0[a+b*layer0_size].weight*beta2;
					bptt_syn0[a+b*layer0_size].weight=0;
					}
			}
			else {
				for (a=layer0_size-layer1_size; a<layer0_size; a++) {
					syn0[a+b*layer0_size].weight+=bptt_syn0[a+b*layer0_size].weight;
					bptt_syn0[a+b*layer0_size].weight=0;
					}
			}
			
			if ((counter%10)==0) {
				for (step=0; step<bptt+bptt_block-2; step++) if (bptt_history[step]!=-1) {
					syn0[bptt_history[step]+b*layer0_size].weight+=bptt_syn0[bptt_history[step]+b*layer0_size].weight - syn0[bptt_history[step]+b*layer0_size].weight*beta2;
					bptt_syn0[bptt_history[step]+b*layer0_size].weight=0;
					}
			}
			else {
				for (step=0; step<bptt+bptt_block-2; step++) if (bptt_history[step]!=-1) {
					syn0[bptt_history[step]+b*layer0_size].weight+=bptt_syn0[bptt_history[step]+b*layer0_size].weight;
				bptt_syn0[bptt_history[step]+b*layer0_size].weight=0;
				}
			}
			}
		}
    }	
}

void CRnnLM::copyHiddenLayerToInput()
{
    int a;

    for (a=0; a<layer1_size; a++) {
        neu0[a+layer0_size-layer1_size].ac=neu1[a].ac;
    }
	//prnnlm
	for (a=0; a<varh_size; a++) {
		neuvv[a+var_size].ac = neuvh[a].ac;
	}
	//end prnnlm
}

void CRnnLM::trainNet()
{
    int a, b, word, last_word, wordcn;
	int varcn;
    char log_name[200];
    FILE *fi, *flog;
    clock_t start, now;

    sprintf(log_name, "%s.output.txt", rnnlm_file);

    printf("Starting training using file %s\n", train_file);
    starting_alpha=alpha;
    
    fi=fopen(rnnlm_file, "rb");
    if (fi!=NULL) {
	fclose(fi);
	printf("Restoring network from file to continue training...\n");
	restoreNet();
    } else {
	learnVocabFromTrainFile();
	initNet();
	iter=0;
    }

    if (class_size>vocab_size) {
	printf("WARNING: number of classes exceeds vocabulary size!\n");
    }
    
    counter=train_cur_pos;
    //saveNet();
    while (iter < maxIter) {
        printf("Iter: %3d\tAlpha: %f\t   ", iter, alpha);
        fflush(stdout);
        
        if (bptt>0) for (a=0; a<bptt+bptt_block; a++) bptt_history[a]=0;
        for (a=0; a<MAX_NGRAM_ORDER; a++) history[a]=0;

        //TRAINING PHASE
        netFlush();

        fi=fopen(train_file, "rb");
        last_word=0;
        
        if (counter>0) for (a=0; a<counter; a++) word=readWordIndex(fi);	//this will skip words that were already learned if the training was interrupted
        
        start=clock();
        
        while (1) {
    	    counter++;
    	    
    	    if ((counter%10000)==0) if ((debug_mode>1)) {
    		now=clock();
    		if (train_words>0)
    		    //printf("%cIter: %3d\tAlpha: %f\t   TRAIN entropy: %.4f    Progress: %.2f%%   Words/sec: %.1f ", 13, iter, alpha, -logp/log10(2)/counter, counter/(real)train_words*100, counter/((double)(now-start)/1000000.0));
				printf("%cIter: %3d\tAlpha: %f\t   TRAIN entropy: %.4f  %.4f   Progress: %.2f%%   Words/sec: %.1f ", 13, iter, alpha, -logp/log10(2)/counter, -vlogp/log10(2)/counter, counter/(real)train_words*100, counter/((double)(now-start)/1000000.0));
    		else
    		    //printf("%cIter: %3d\tAlpha: %f\t   TRAIN entropy: %.4f    Progress: %dK", 13, iter, alpha, -logp/log10(2)/counter, counter/1000);
				printf("%cIter: %3d\tAlpha: %f\t   TRAIN entropy: %.4f  %.4f   Progress: %dK", 13, iter, alpha, -logp/log10(2)/counter, -vlogp/log10(2)/counter, counter/1000);
    		fflush(stdout);
    	    }
    	    
    	    if ((anti_k>0) && ((counter%anti_k)==0)) {
    		train_cur_pos=counter;
    		saveNet();
    	    }
       
            var_update=false;  //prnnlm 
			word=readWordIndex(fi);     //read next word
            computeNet(last_word, word);      //compute probability distribution
            if (feof(fi)) break;        //end of file: test on validation data, iterate till convergence

            //if (word!=-1) 
            //    logp+=log10(neu2[vocab[word].class_index+vocab_size].ac * neu2[word].ac);
			if (word!=-1) {	//prnnlm
				double logpp=log10(neu2[vocab[word].class_index+vocab_size].ac) + log10(neu2[word].ac);
				if ((logp!=logp) || (isinf(logpp))){
					cout<<"avoided numberical error."<<endl;
					logpp=log10(neu2[vocab[word].class_index+vocab_size].ac*100000000000000000000)+log10(neu2[word].ac*100000000000000000000)-log10(100000000000000000000)*2;
					logp+=logpp;
				} else {
					logp+=logpp;
				}               
    	    }
			
			if (var!=-1) {	//prnnlm
				vlogp+=log10(neuv[var].ac);
			}
			
			if (isinf(vlogp)) {	//prnnlm
				printf("\nVar numerical error %d %f\n", var, neuv[var].ac);
				exit(1);
			}
			
    	    if ((logp!=logp) || (isinf(logp))) {
    	        printf("\nNumerical error %d %f %f\n", word, neu2[word].ac, neu2[vocab[word].class_index+vocab_size].ac);
    	        exit(1);
    	    }
	    
            //
            if (bptt>0) {		//shift memory needed for bptt to next time step
				for (a=bptt+bptt_block-1; a>0; a--) bptt_history[a]=bptt_history[a-1];
				bptt_history[0]=last_word;
		
				for (a=bptt+bptt_block-1; a>0; a--) for (b=0; b<layer1_size; b++) {
					bptt_hidden[a*layer1_size+b].ac=bptt_hidden[(a-1)*layer1_size+b].ac;
					bptt_hidden[a*layer1_size+b].er=bptt_hidden[(a-1)*layer1_size+b].er;
				}	
			}
            //
            learnNet(last_word, word);
            
            var_update=false; //prnnlm

            copyHiddenLayerToInput();

            if (last_word!=-1) neu0[last_word].ac=0;  //delete previous activation

            last_word=word;
            
            for (a=MAX_NGRAM_ORDER-1; a>0; a--) history[a]=history[a-1];
            history[0]=last_word;

			if (independent && (word==0)) netReset();
        }
        fclose(fi);

		now=clock();
    	printf("%cIter: %3d\tAlpha: %f\t   TRAIN entropy: %.4f  %.4f  Words/sec: %.1f   ", 13, iter, alpha, -logp/log10(2)/counter, -vlogp/log10(2)/counter, counter/((double)(now-start)/1000000.0));
   
    	if (one_iter==1) {	//no validation data are needed and network is always saved with modified weights
    	    printf("\n");
			logp=0;
    	    saveNet();
            break;
    	}

        //VALIDATION PHASE
        netFlush();

        fi=fopen(valid_file, "rb");
		if (fi==NULL) {
			printf("Valid file not found\n");
			exit(1);
		}
        
        flog=fopen(log_name, "ab");
		if (flog==NULL) {
			printf("Cannot open log file\n");
			exit(1);
		}
        
        //fprintf(flog, "Index   P(NET)          Word\n");
        //fprintf(flog, "----------------------------------\n");
        
        last_word=0;
        logp=0;
        wordcn=0;
		vlogp=0; varcn=0;//prnnlm
        while (1) {
            var_update=false;  //prnnlm
            word=readWordIndex(fi);     //read next word
            computeNet(last_word, word);      //compute probability distribution
            if (feof(fi)) break;        //end of file: report LOGP, PPL
            
    	    if (word!=-1) {
				logp+=log10(neu2[vocab[word].class_index+vocab_size].ac * neu2[word].ac);
				wordcn++;
    	    }
			
			if (var!=-1) {//prnnlm
				vlogp+=log10(neuv[var].ac);
				varcn++;
			}

            /*if (word!=-1)
                fprintf(flog, "%d\t%f\t%s\n", word, neu2[word].ac, vocab[word].word);
            else
                fprintf(flog, "-1\t0\t\tOOV\n");*/

            //learnNet(last_word, word);    //*** this will be in implemented for dynamic models
            copyHiddenLayerToInput();

            if (last_word!=-1) neu0[last_word].ac=0;  //delete previous activation

            last_word=word;
            
            for (a=MAX_NGRAM_ORDER-1; a>0; a--) history[a]=history[a-1];
            history[0]=last_word;

			if (independent && (word==0)) netReset();
        }
        fclose(fi);
        
        fprintf(flog, "\niter: %d\n", iter);
        fprintf(flog, "valid log probability: %f  %f\n", logp, vlogp);
		ppl=exp10(-logp/(real)wordcn);
        fprintf(flog, "PPL net: %f  %f\n", ppl, exp10(-vlogp/(real)varcn));
        
        fclose(flog);
    
        printf("VALID entropy: %.4f  %.4f\n", -logp/log10(2)/wordcn, -vlogp/log10(2)/varcn);
        
        counter=0;
		train_cur_pos=0;

        if (logp<llogp)
            restoreWeights();
        else
            saveWeights();
		
		if (vlogp<vllogp)	//prnnlm
            restoreVarWeights();
        else
            saveVarWeights();

        /*if (logp*min_improvement<llogp) {
            if (alpha_divide==0) alpha_divide=1;
            else if(llogp*min_improvement<lllogp){
                saveNet();
                break;
            }
        }*/
		
		if (ppl+0.2>lppl) {
            if (alpha_divide==0) alpha_divide=1;
            else if(lppl+0.2>llppl){
                saveNet();
                break;
            }
        }

        if (alpha_divide) alpha/=2;

		lllogp=llogp;//prnnlm
        llogp=logp;
		
        logp=0;
		//prnnlm
		vllogp=vlogp;
		vlogp=0;
		
		llppl=lppl;
		lppl=ppl;
		//end prnnlm
		
        iter++;
        saveNet();
    }
}

void CRnnLM::testNet()
{
    int a, b, word, last_word, wordcn;
    FILE *fi, *flog, *lmprob=NULL;
    real prob_other, log_other, log_combine;
    double d;
    
    restoreNet();
    
    if (use_lmprob) {
	lmprob=fopen(lmprob_file, "rb");
    }

    //TEST PHASE
    //netFlush();

    fi=fopen(test_file, "rb");
    //sprintf(str, "%s.%s.output.txt", rnnlm_file, test_file);
    //flog=fopen(str, "wb");
    flog=stdout;

    if (debug_mode>1)	{
	if (use_lmprob) {
    	    fprintf(flog, "Index   P(NET)          P(LM)           Word\n");
    	    fprintf(flog, "--------------------------------------------------\n");
	} else {
    	    fprintf(flog, "Index   P(NET)          Word\n");
    	    fprintf(flog, "----------------------------------\n");
	}
    }

    last_word=0;					//last word = end of sentence
    logp=0;
    log_other=0;
    log_combine=0;
    prob_other=0;
    wordcn=0;
    copyHiddenLayerToInput();
    
    if (bptt>0) for (a=0; a<bptt+bptt_block; a++) bptt_history[a]=0;
    for (a=0; a<MAX_NGRAM_ORDER; a++) history[a]=0;
    if (independent) netReset();
    
    while (1) {
        var_update=false; //prnnlm
        word=readWordIndex(fi);		//read next word
        computeNet(last_word, word);		//compute probability distribution
        if (feof(fi)) break;		//end of file: report LOGP, PPL
        
        if (use_lmprob) {
            fscanf(lmprob, "%lf", &d);
    	    prob_other=d;

            goToDelimiter('\n', lmprob);
        }

        if ((word!=-1) || (prob_other>0)) {
    	    if (word==-1) {
    		logp+=-8;		//some ad hoc penalty - when mixing different vocabularies, single model score is not real PPL
        	log_combine+=log10(0 * lambda + prob_other*(1-lambda));
    	    } else {
    		logp+=log10(neu2[vocab[word].class_index+vocab_size].ac * neu2[word].ac);
        	log_combine+=log10(neu2[vocab[word].class_index+vocab_size].ac * neu2[word].ac*lambda + prob_other*(1-lambda));
    	    }
    	    log_other+=log10(prob_other);
            wordcn++;
        }

		if (debug_mode>1) {
    	    if (use_lmprob) {
        	if (word!=-1) fprintf(flog, "%d\t%.10f\t%.10f\t%s", word, neu2[vocab[word].class_index+vocab_size].ac *neu2[word].ac, prob_other, vocab[word].word);
        	else fprintf(flog, "-1\t0\t\t0\t\tOOV");
    	    } else {
        	if (word!=-1) fprintf(flog, "%d\t%.10f\t%s", word, neu2[vocab[word].class_index+vocab_size].ac *neu2[word].ac, vocab[word].word);
        	else fprintf(flog, "-1\t0\t\tOOV");
    	    }
    	    
    	    fprintf(flog, "\n");
    	}

        if (dynamic>0) {
            if (bptt>0) {
                for (a=bptt+bptt_block-1; a>0; a--) bptt_history[a]=bptt_history[a-1];
                bptt_history[0]=last_word;
                                    
                for (a=bptt+bptt_block-1; a>0; a--) for (b=0; b<layer1_size; b++) {
                    bptt_hidden[a*layer1_size+b].ac=bptt_hidden[(a-1)*layer1_size+b].ac;
                    bptt_hidden[a*layer1_size+b].er=bptt_hidden[(a-1)*layer1_size+b].er;
        	}
            }
            //
            alpha=dynamic;
    	    learnNet(last_word, word);    //dynamic update
            var_update=false; //prnnlm
    	}
        copyHiddenLayerToInput();
        
        if (last_word!=-1) neu0[last_word].ac=0;  //delete previous activation

        last_word=word;
        
        for (a=MAX_NGRAM_ORDER-1; a>0; a--) history[a]=history[a-1];
        history[0]=last_word;

	if (independent && (word==0)) netReset();
    } //end while
    fclose(fi);
    if (use_lmprob) fclose(lmprob);

    //write to log file
    if (debug_mode>0) {
	fprintf(flog, "\ntest log probability: %f\n", logp);
	if (use_lmprob) {
    	    fprintf(flog, "test log probability given by other lm: %f\n", log_other);
    	    fprintf(flog, "test log probability %f*rnn + %f*other_lm: %f\n", lambda, 1-lambda, log_combine);
	}

	fprintf(flog, "\nPPL net: %f\n", exp10(-logp/(real)wordcn));
	if (use_lmprob) {
    	    fprintf(flog, "PPL other: %f\n", exp10(-log_other/(real)wordcn));
    	    fprintf(flog, "PPL combine: %f\n", exp10(-log_combine/(real)wordcn));
	}
    }
    printf("end testNet()!"); //prnnlm 
    fclose(flog);
}

void CRnnLM::testNbest()
{
    int a, word, last_word, wordcn;
    FILE *fi, *flog, *lmprob=NULL;
    float prob_other; //has to be float so that %f works in fscanf
    real log_other, log_combine, senp;
    //int nbest=-1;
    int nbest_cn=0;
    char ut1[MAX_STRING], ut2[MAX_STRING];

    restoreNet();
    computeNet(0, 0);
    copyHiddenLayerToInput();
    saveContext();
    saveContext2();
    
    if (use_lmprob) {
	lmprob=fopen(lmprob_file, "rb");
    } else lambda=1;		//!!! for simpler implementation later

    //TEST PHASE
    //netFlush();
    
    for (a=0; a<MAX_NGRAM_ORDER; a++) history[a]=0;

    if (!strcmp(test_file, "-")) fi=stdin; else fi=fopen(test_file, "rb");
    
    //sprintf(str, "%s.%s.output.txt", rnnlm_file, test_file);
    //flog=fopen(str, "wb");
    flog=stdout;

    last_word=0;		//last word = end of sentence
    logp=0;
    log_other=0;
    prob_other=0;
    log_combine=0;
    wordcn=0;
    senp=0;
    strcpy(ut1, (char *)"");
    while (1) {
	if (last_word==0) {
	    fscanf(fi, "%s", ut2);
	    
	    if (nbest_cn==1) saveContext2();		//save context after processing first sentence in nbest
	    
	    if (strcmp(ut1, ut2)) {
		strcpy(ut1, ut2);
		nbest_cn=0;
		restoreContext2();
		saveContext();
	    } else restoreContext();
	    
	    nbest_cn++;
	    
	    copyHiddenLayerToInput();
        }
    
	var_update=false; //prnnlm
	word=readWordIndex(fi);     //read next word
	if (lambda>0) computeNet(last_word, word);      //compute probability distribution
        if (feof(fi)) break;        //end of file: report LOGP, PPL
        
        
        if (use_lmprob) {
            fscanf(lmprob, "%f", &prob_other);
            goToDelimiter('\n', lmprob);
        }
        
        if (word!=-1)
        neu2[word].ac*=neu2[vocab[word].class_index+vocab_size].ac;
        
        if (word!=-1) {
            logp+=log10(neu2[word].ac);
    	    
            log_other+=log10(prob_other);
            
            log_combine+=log10(neu2[word].ac*lambda + prob_other*(1-lambda));
            
            senp+=log10(neu2[word].ac*lambda + prob_other*(1-lambda));
            
            wordcn++;
        } else {
    	    //assign to OOVs some score to correctly rescore nbest lists, reasonable value can be less than 1/|V| or backoff LM score (in case it is trained on more data)
    	    //this means that PPL results from nbest list rescoring are not true probabilities anymore (as in open vocabulary LMs)
    	    
    	    real oov_penalty=-5;	//log penalty
    	    
    	    if (prob_other!=0) {
    		logp+=log10(prob_other);
    		log_other+=log10(prob_other);
    		log_combine+=log10(prob_other);
    		senp+=log10(prob_other);
    	    } else {
    		logp+=oov_penalty;
    		log_other+=oov_penalty;
    		log_combine+=oov_penalty;
    		senp+=oov_penalty;
    	    }
    	    wordcn++;
        }
        
        //learnNet(last_word, word);    //*** this will be in implemented for dynamic models
        copyHiddenLayerToInput();

        if (last_word!=-1) neu0[last_word].ac=0;  //delete previous activation
        
        if (word==0) {		//write last sentence log probability / likelihood
    	    fprintf(flog, "%f\n", senp);
    	    senp=0;
	}

        last_word=word;
        
        for (a=MAX_NGRAM_ORDER-1; a>0; a--) history[a]=history[a-1];
        history[0]=last_word;

	if (independent && (word==0)) netReset();
    }
    fclose(fi);
    if (use_lmprob) fclose(lmprob);

    if (debug_mode>0) {
	printf("\ntest log probability: %f\n", logp);
	if (use_lmprob) {
    	    printf("test log probability given by other lm: %f\n", log_other);
    	    printf("test log probability %f*rnn + %f*other_lm: %f\n", lambda, 1-lambda, log_combine);
	}

	printf("\nPPL net: %f\n", exp10(-logp/(real)wordcn));
	if (use_lmprob) {
    	    printf("PPL other: %f\n", exp10(-log_other/(real)wordcn));
    	    printf("PPL combine: %f\n", exp10(-log_combine/(real)wordcn));
	}
    }

    fclose(flog);
}

void CRnnLM::testGen()
{
    int i, word, cla, last_word, wordcn, c, b, a=0;
    real f, g, sum;
    
    restoreNet();
    
    word=0;
    last_word=0;					//last word = end of sentence
    wordcn=0;
    copyHiddenLayerToInput();
    while (wordcn<gen) {
        computeNet(last_word, 0);		//compute probability distribution
        
        f=random(0, 1);
        g=0;
        i=vocab_size;
        while ((g<f) && (i<layer2_size)) {
    	    g+=neu2[i].ac;
    	    i++;
        }
        cla=i-1-vocab_size;
        
        if (cla>class_size-1) cla=class_size-1;
        if (cla<0) cla=0;
        
        //
        // !!!!!!!!  THIS WILL WORK ONLY IF CLASSES ARE CONTINUALLY DEFINED IN VOCAB !!! (like class 10 = words 11 12 13; not 11 12 16)  !!!!!!!!
        // forward pass 1->2 for words
        for (c=0; c<class_cn[cla]; c++) neu2[class_words[cla][c]].ac=0;
        matrixXvector(neu2, neu1, syn1, layer1_size, class_words[cla][0], class_words[cla][0]+class_cn[cla], 0, layer1_size, 0);
        
        //apply direct connections to words
	if (word!=-1) if (direct_size>0) {
    	    unsigned long long hash[MAX_NGRAM_ORDER];

            for (a=0; a<direct_order; a++) hash[a]=0;

            for (a=0; a<direct_order; a++) {
                b=0;
                if (a>0) if (history[a-1]==-1) break;
                hash[a]=PRIMES[0]*PRIMES[1]*(unsigned long long)(cla+1);

                for (b=1; b<=a; b++) hash[a]+=PRIMES[(a*PRIMES[b]+b)%PRIMES_SIZE]*(unsigned long long)(history[b-1]+1);
                hash[a]=(hash[a]%(direct_size/2))+(direct_size)/2;
    	    }

    	    for (c=0; c<class_cn[cla]; c++) {
        	a=class_words[cla][c];

        	for (b=0; b<direct_order; b++) if (hash[b]) {
    		    neu2[a].ac+=syn_d[hash[b]];
            	    hash[b]++;
        	    hash[b]=hash[b]%direct_size;
    	        } else break;
    	    }
	}
        
        //activation 2   --softmax on words
        // 130425 - this is now a 'safe' softmax

        sum=0;
        real maxAc=-FLT_MAX;
      	for (c=0; c<class_cn[cla]; c++) {
      	    a=class_words[cla][c];
            if (neu2[a].ac>maxAc) maxAc=neu2[a].ac;
        }
        for (c=0; c<class_cn[cla]; c++) {
            a=class_words[cla][c];
            sum+=fasterexp(neu2[a].ac-maxAc);
        }
        for (c=0; c<class_cn[cla]; c++) {
            a=class_words[cla][c];
            neu2[a].ac=fasterexp(neu2[a].ac-maxAc)/sum; //this prevents the need to check for overflow
        }
	//
	
	f=random(0, 1);
        g=0;
        /*i=0;
        while ((g<f) && (i<vocab_size)) {
    	    g+=neu2[i].ac;
    	    i++;
        }*/
        for (c=0; c<class_cn[cla]; c++) {
    	    a=class_words[cla][c];
    	    g+=neu2[a].ac;
    	    if (g>f) break;
        }
        word=a;
        
	if (word>vocab_size-1) word=vocab_size-1;
        if (word<0) word=0;

	//printf("%s %d %d\n", vocab[word].word, cla, word);
	if (word!=0)
	    printf("%s ", vocab[word].word);
	else
	    printf("\n");

        copyHiddenLayerToInput();

        if (last_word!=-1) neu0[last_word].ac=0;  //delete previous activation

        last_word=word;
        
        for (a=MAX_NGRAM_ORDER-1; a>0; a--) history[a]=history[a-1];
        history[0]=last_word;

	if (independent && (word==0)) netReset();
        
        wordcn++;
    }
}
