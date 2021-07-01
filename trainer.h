#ifndef TRAINER_H
#define TRAINER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

struct params {
	float sample_rate;
	float time;
	float *freqs;
	float *phases;
	float *amps;
};

typedef struct params * PARAMS;

struct signal {
	float *t;
	float *s;
	int points;
	float sample_rate;
	char name[1024];
};

typedef struct signal * SIGNAL;

struct layer {
	int isinput;
	int neurons;
	float *weights;
	float *biases;
	float *outputs;
	float **prev_outputs;
	float *deltas;
	struct layer *prev;
	struct layer *next;
};

void dump_signal(SIGNAL mysignal);
void dump_weights (struct layer *mlp,int stride);
void generate_synthetic_data (PARAMS myparams,SIGNAL mysignal);
void forward_pass (struct layer *mlp,int debug);
void backward_pass (struct layer *mlp,float *y);
void update_weights (struct layer *mlp,float alpha);
void subsample (SIGNAL in_signal,SIGNAL out_signal,float subsample_rate);
void initialize_signal_parameters (PARAMS myparams);
void initialize_mlp (struct layer *layers,int num_layers,int *layer_sizes);
void plot (SIGNAL mysignal,char *title,int n,float time,FILE *dump_file);
void free_signal (SIGNAL mysignal);
void dump_weights_as_constants(struct layer *layers);
void dump_signal_as_constant (SIGNAL mysignal,const char *name);
void synthesize_input_signal(SIGNAL *mysignal,SIGNAL *mysignal_subsampled);
void initialize_output_signal(SIGNAL mysignal_predicted,SIGNAL mysignal_subsampled);
void online_training (struct layer *layers,SIGNAL mysignal_subsampled,SIGNAL mysignal_predicted);
void plot_results(SIGNAL mysignal_subsampled,SIGNAL mysignal_predicted);
void eval_network(struct layer *layers,SIGNAL mysignal_subsampled,SIGNAL mysignal_predicted_offline);
void free_signal (SIGNAL mysignal);
int train_network (struct layer *layers,int num_layers,int *layer_sizes,int num_epochs,SIGNAL *input_signal,SIGNAL *output_signal_expected);
void gen_testbench (FILE *myFile,SIGNAL input_signal,SIGNAL output_signal_expected);

#endif
