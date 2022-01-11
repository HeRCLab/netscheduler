#include "trainer.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "netscheduler.h"
#include "trainer.h"

void dump_signal(SIGNAL mysignal) {
	FILE *myFile;

	myFile=fopen("input_signal.txt","w+");
	if (!myFile) {
		perror("ERROR: cannot open signal file for writing");
		exit(1);
	}

	for (int i=0;i<mysignal->points;i++) {
		float sigval = mysignal->s[i];
		unsigned int hex = *((unsigned int *)&sigval) >> 16;
		fprintf(myFile,"%04X\n",hex);
	}

	fclose(myFile);
}

void dump_weights (struct layer *mlp,int stride) {
	struct layer *current_layer;
	FILE *myFile;
	char str[1024];

	for (int i=0;i<stride;i++) {
		snprintf(str,1024,"weights%d.txt",i);
		myFile=fopen(str,"w+");
		if (!myFile) {
			perror("ERROR: cannot open weights file for writing");
			exit(1);
		}

		current_layer=mlp;
		while (current_layer=current_layer->next) {
			for (int j=i;j<current_layer->neurons;j+=stride) {
				int inputs = current_layer->prev->neurons;
				for (int k=0;k<inputs;k++) {
					float weight = current_layer->weights[j*inputs+k];
					unsigned int hex = *((unsigned int *)&weight) >> 16;
					fprintf(myFile,"%04X\n",hex);
				}
			}
		}
		fclose(myFile);
	}
	
}

void generate_synthetic_data (PARAMS myparams,SIGNAL mysignal) {
	// generate baseline time array
	int points = (int)ceilf(myparams->sample_rate * myparams->time);
	float sample_period = 1.f / myparams->sample_rate;
	
	// allocate time and signal arrays
	mysignal->s = (float *)malloc(sizeof(float) * points);
	mysignal->t = (float *)malloc(sizeof(float) * points);

	// synthesize
	for (int i=0;i<points;i++) {
		mysignal->s[i]=0.f;
		mysignal->t[i]=sample_period * (float)i;
		for (int j=0;myparams->freqs[j]!=0.f;j++) {
			mysignal->s[i] += myparams->amps[j]*sinf(2.f*M_PI*myparams->freqs[j]*mysignal->t[i] + myparams->phases[j]);
		}
	}

	mysignal->points = points;
	
	mysignal->sample_rate = myparams->sample_rate;
}

void read_signal (const char *filename,SIGNAL mysignal) {
	FILE *myFile=fopen(filename,"r+");
	
	if (!myFile) {
		char str[1024];
		snprintf(str,1024,"Error opening \"%s\" for read",filename);
		perror(str);
		exit(1);
	}
	
	int records=0;
	int ret;
	float time,voltage,force,acceleration;
	ret = fscanf(myFile,"%f %f %f %f",&time,&voltage,&force,&acceleration);
	while (ret!=EOF) {
#ifdef SIGNAL_TIME_END
		if (time > SIGNAL_TIME_END)
			break;
#endif
		records++;
		ret = fscanf(myFile,"%f %f %f %f",&time,&voltage,&force,&acceleration);
	}
	
	mysignal->points = records;
	mysignal->sample_rate = (float)records / time;
	
	logmsg("Input signal read from \"%s\", records = %d, time = %0.4f s, sample rate = %0.3f samples/sec",filename,records,time,mysignal->sample_rate);
	
	mysignal->t = (float *)malloc(sizeof(float) * records);
	mysignal->s = (float *)malloc(sizeof(float) * records);
	strcpy(mysignal->name,filename);
	
	fseek(myFile,0,SEEK_SET);
	for (int i=0;i<records;i++)
		fscanf(myFile,"%f %f %f %f",&mysignal->t[i],&voltage,&force,&mysignal->s[i]);
}

void shift_prev_outputs (struct layer *mlp) {
	struct layer *current_layer=mlp;

	// for each layer
	while (current_layer) {
		
		// for each shift register
		for (int i=FORECAST_LENGTH-1;i>=0;i--) {
			// for each neuron
			for (int j=0;j<current_layer->neurons;j++) {
				if (i==0)
					current_layer->prev_outputs[0][j] = current_layer->outputs[j];
				else
					current_layer->prev_outputs[i][j] = current_layer->prev_outputs[i-1][j];
			}
		}
		
		current_layer=current_layer->next;
	}
}

void forward_pass (struct layer *mlp,int debug) {
	// start with second layer, since the first layer is the input layer and there's
	// nothing to do there
	struct layer *current_layer=mlp;

	int layer = 0;

	while (current_layer=current_layer->next) {
		if (debug) logmsg("LAYER %d:",layer++);
		// matrix-vector multiply
		for (int i=0;i<current_layer->neurons;i++) {
			float sum=0.f;
			for (int j=0;j<current_layer->prev->neurons;j++)
				sum+=current_layer->prev->outputs[j] * current_layer->weights[i*current_layer->prev->neurons+j];
			current_layer->outputs[i]=sum+current_layer->biases[i];
			
			if (debug) logmsg("output %d = %0.4e",i,current_layer->outputs[i]);
		}
	}
}

void backward_pass (struct layer *mlp,float *y) {
	// skip to last layer
	struct layer *current_layer=mlp;
	while (current_layer->next) current_layer=current_layer->next;

	// handle last layer separately
	for (int i=0;i<current_layer->neurons;i++) current_layer->deltas[i]=current_layer->outputs[i]-y[i];
	//for (int i=0;i<current_layer->neurons;i++)
		//current_layer->deltas[i]=current_layer->prev_outputs[FORECAST_LENGTH-1][i]-y[i];
	
	current_layer=current_layer->prev;

	while (current_layer->prev) {

		for (int i=0;i<current_layer->neurons;i++) {
			float sum=0.f;
			int neurons_in_layer = current_layer->neurons;
			for (int j=0;j<current_layer->next->neurons;j++) {
				sum+=current_layer->next->deltas[j]*current_layer->next->weights[j*neurons_in_layer+i];
				//printf ("sum += deltas[next][%d] * weights[next][%d];\n",j,j);
			}
			
			current_layer->deltas[i]=current_layer->outputs[i]*sum;
			//current_layer->deltas[i]=current_layer->prev_outputs[FORECAST_LENGTH-1][i]*sum;
			
			//printf ("deltas[current][%d] = outputs[current][%d] * sum = %e;\n",i,i,current_layer->deltas[i]);
		}

		current_layer=current_layer->prev;
	}
}

void update_weights (struct layer *mlp,float alpha) {
	struct layer *current_layer = mlp->next;
	
	while (current_layer) {
		for (int i=0;i<current_layer->neurons;i++) {
			for (int j=0;j<current_layer->prev->neurons;j++) {
				current_layer->weights[i*current_layer->prev->neurons+j] -=
					alpha * current_layer->deltas[i] * current_layer->prev->outputs[j];
					
				//printf ("weights[current][%d] -= alpha * deltas[current][%d] * outputs[prev][%d];\n",j,i,j);
			}
			current_layer->biases[i] -= alpha * current_layer->deltas[i];
		}
		current_layer = current_layer->next;
	}
}

void subsample (SIGNAL in_signal,SIGNAL out_signal,float subsample_rate) {
	int len = in_signal->points;
	int len_new = out_signal->points = ceil(len / subsample_rate);
	
	out_signal->sample_rate = in_signal->sample_rate/subsample_rate;
	// allocate time and signal arrays
	out_signal->s = (float *)malloc(sizeof(float) * len_new);
	out_signal->t = (float *)malloc(sizeof(float) * len_new);
	
	for (int i=0;i<len_new;i++) {
		float position = (float)i * subsample_rate;
		float position_frac = position - floorf(position);
		int position_int = (int)floorf(position);
		out_signal->s[i] = (1.f - position_frac)*
					in_signal->s[position_int] +
					position_frac*
					in_signal->s[position_int+1];
		out_signal->t[i] = (float)i / out_signal->sample_rate;
	}
}

void initialize_signal_parameters (PARAMS myparams) {
	myparams->freqs = (float*)malloc(sizeof(float)*4);
	myparams->freqs[0]=10;
	myparams->freqs[1]=37;
	myparams->freqs[2]=78;
	myparams->freqs[3]=0;
	myparams->phases = (float*)malloc(sizeof(float)*4);
	myparams->phases[0]=0;
	myparams->phases[1]=1;
	myparams->phases[2]=2;
	myparams->phases[3]=0;
	myparams->amps = (float*)malloc(sizeof(float)*4);
	myparams->amps[0]=1;
	myparams->amps[1]=2;
	myparams->amps[2]=3;
	myparams->amps[3]=4;
	myparams->time = SYNTHESIZED_SIGNAL_TIME;
	myparams->sample_rate=SAMPLE_RATE;
}

void initialize_mlp (struct layer *layers,int num_layers,int *layer_sizes) {
	
	for (int i=0;i<num_layers;i++) {
		layers[i].isinput=(i==0) ? 1 : 0;
		layers[i].neurons=layer_sizes[i];
		layers[i].prev=(i==0) ? 0 : &layers[i-1];
		layers[i].next=(i==num_layers-1) ? 0 : &layers[i+1];
		layers[i].outputs=(float *)malloc(sizeof(float)*layer_sizes[i]);
		layers[i].weights=(i==0) ? 0 : (float*)malloc(sizeof(float)*layer_sizes[i]*layer_sizes[i-1]);
		layers[i].deltas=(i==0) ? 0 : (float *)malloc(sizeof(float)*layer_sizes[i]);
		layers[i].biases=(i==0) ? 0 : (float *)malloc(sizeof(float)*layer_sizes[i]);
		if (i>0) {
			for (int j=0;j<layer_sizes[i]*layer_sizes[i-1];j++) {
				layers[i].weights[j]=((float)rand()/(float)RAND_MAX - 0.5f) * INITIAL_WEIGHT_SCALER;
			}
			for (int j=0;j<layer_sizes[i];j++) {
				layers[i].biases[j]=0.f;
			}
		}
		
		layers[i].prev_outputs = (float **)malloc(sizeof(float *)*FORECAST_LENGTH);
		for (int j=0;j<FORECAST_LENGTH;j++) {
			layers[i].prev_outputs[j] = (float *)malloc(sizeof(float)*layer_sizes[i]);
			for (int k=0;k<layer_sizes[i];k++) layers[i].prev_outputs[j][k] = 0.f;
		}
	}
}

void plot (SIGNAL mysignal,const char *title,int n,float time,FILE *dump_file) {
	// dump signal
	char str[4096];
	FILE *myFile[1024];

	// open a file to dump each trace
	for (int i=0;i<n;i++) {
		snprintf(str,1024,"data%d.txt",i);
		myFile[i] = fopen(str,"w+");
		if (!myFile[i]) {
			perror("ERROR: can't create temp file in PWD");
			exit(1);
		}
	}

	// write each trace to a file
	if (dump_file) {
		fprintf(dump_file,"\"time\"");
		for (int j=0;j<n;j++) {
			fprintf(dump_file,",\"%s\"",mysignal[j].name);
		}
		fprintf(dump_file,"\n");
	}
	
	for (int i=0;i<mysignal->points;i++) {
		
		if (dump_file) fprintf(dump_file,"%0.12e",mysignal[0].t[i]);
		
		for (int j=0;j<n;j++) {
			if (mysignal[j].t[i] < time)
				fprintf (myFile[j],"%0.12f %0.12f\n",mysignal[j].t[i],mysignal[j].s[i]);
			
			if (dump_file) {
				fprintf(dump_file,",%0.12f",mysignal[j].s[i]);
			}
		}
		
		if (dump_file) fprintf(dump_file,"\n");
	}
	
	// close all files
	for (int i=0;i<n;i++) fclose(myFile[i]);

	// set up plot command
	// add -p for persist
	snprintf(str,1024,"/usr/bin/gnuplot -p -e \""
			"set title '%s';"
			"set xlabel 'time (s)';"
			"set ylabel 'accel';"
			//"set key off;"
			"plot ",title);

	for (int i=0;i<n;i++) {
		char temp[1024];
		
		snprintf (temp,1024,"'data%d.txt' title '%s' with lines",i,mysignal[i].name);
		if (i<(n-1)) strcat(temp,",");
		strcat(str,temp);
	}
	strcat(str,";\"");

	FILE *myplot = popen(str,"w");
	
	if (!myplot) {
		perror("Error opening gnuplot");
		exit(1);
	}
	
	pclose(myplot);
}

void free_signal (SIGNAL mysignal) {
	free(mysignal->t);
	free(mysignal->s);
}

SIGNAL prepare_training_data () {
	// set up signal
	PARAMS myparams = (PARAMS)malloc(sizeof(struct params));
	initialize_signal_parameters(myparams);
		
	// synthesize and plot signal
	SIGNAL mysignal = (SIGNAL)malloc(sizeof(struct signal));
	
#ifdef USE_SIGNAL_FILE
	read_signal (USE_SIGNAL_FILE,mysignal);
#else
	generate_synthetic_data (myparams,mysignal);
#endif

	strcpy(mysignal->name,"original signal");
	plot(mysignal,"original signal",1,mysignal->t[mysignal->points-1],0);
	
	float native_sample_rate = mysignal->points / mysignal->t[mysignal->points-1];	
	logmsg("Subsampling input signal from %0.3f to %0.3f samples/sec",native_sample_rate,(float)SUBSAMPLED_RATE);
	
	// create subsampled signal
	SIGNAL mysignal_subsampled = (SIGNAL)malloc(sizeof(struct signal));
	subsample(mysignal,mysignal_subsampled,native_sample_rate/(float)SUBSAMPLED_RATE);
	
	// free original signal
	//free(mysignal->t);
	//free(mysignal->s);
	//free(mysignal);
	
	// plot
	strcpy(mysignal_subsampled->name,"original signal subsampled");
	plot(mysignal_subsampled,"original signal subsampled",1,
				mysignal_subsampled->t[mysignal_subsampled->points-1],0);
				
	// free up myparams and exit
	
	logmsg("Subsampled signal has %d records and time %0.3f s (%0.3f samples/second)",
				mysignal_subsampled->points,
				mysignal_subsampled->t[mysignal_subsampled->points-1],
				mysignal_subsampled->points / mysignal_subsampled->t[mysignal_subsampled->points-1]);
	
	free(myparams->freqs);
	free(myparams->phases);
	free(myparams);
	return mysignal_subsampled;
}

int train_network (struct layer *layers,struct layer *initial_trainer_layers,int num_layers,int *layer_sizes,int num_epochs,SIGNAL *input_signal,SIGNAL *output_signal_expected) {

	*input_signal = prepare_training_data ();
	initialize_mlp(layers,num_layers,layer_sizes);
	initialize_mlp(initial_trainer_layers,num_layers,layer_sizes);
	
	// copy the initial state of the network to the "initial" version of the layers
	// this way, we can initialize the weights in the hardware to match what they
	// where before we performed training in the software
	for (int i=1;i<NUM_LAYERS;i++) {
		memcpy(initial_trainer_layers[i].weights,layers[i].weights,sizeof(float)*layer_sizes[i]*layer_sizes[i-1]);
		memcpy(initial_trainer_layers[i].biases,layers[i].biases,sizeof(float)*layer_sizes[i]);
	}
	
	int num_samples = (*input_signal)->points;

	// set up predicted signal
	(*output_signal_expected)=(SIGNAL)malloc(sizeof(struct signal));
	(*output_signal_expected)->sample_rate = (*input_signal)->sample_rate;
	strcpy((*output_signal_expected)->name,"predicted");
	// set number of points to that of subsampled signal
	(*output_signal_expected)->points = num_samples;
	// allocate time axis
	(*output_signal_expected)->t = (float *)malloc(num_samples * sizeof(float));
	// copy time axis from subsampled signal
	memcpy((*output_signal_expected)->t,(*input_signal)->t,num_samples * sizeof(float));
	// allocate y axis
	(*output_signal_expected)->s = (float *)malloc(num_samples * sizeof(float));

	int history_length = layer_sizes[0];
	// zero-pad on the left side
	for (int i=0;i<history_length+FORECAST_LENGTH;i++) {
	//for (int i=0;i<history_length;i++) {
		(*output_signal_expected)->s[i] = 0.f;
	}
	
#ifdef PERFORM_OFFLINE_TRAINING
	
	// train it
	float learning_rate = LEARNING_RATE;
	
	// allocate memory for input array
	float *inputs = (float *)malloc(sizeof(float)*HISTORY_LENGTH);
	layers[0].outputs = inputs;
	
	// zero-pad output array
	for (int i=0;i<FORECAST_LENGTH;i++)
		(*output_signal_expected)->s[i]=0.f;
		
	// learning rate schedule
	//if (epoch % (num_epochs / 4) == 0) learning_rate = learning_rate / 10.f;
	
	logmsg("Training MLP...");
	
	// perform training
	for (int i=0;i<num_samples;i++) {
			
		// make prediction using current inputs
		for (int j=0;j<HISTORY_LENGTH;j++) {
			inputs[j]=i-HISTORY_LENGTH+j+1<0 ? 0.f : (*input_signal)->s[i-history_length+j+1];
		}
		forward_pass(layers,0);
		if (i+FORECAST_LENGTH < num_samples)
			(*output_signal_expected)->s[i]=layers[num_layers-1].outputs[0];
		
		// make prediction using past inputs
		for (int j=0;j<HISTORY_LENGTH;j++)
			inputs[j]=i-HISTORY_LENGTH-FORECAST_LENGTH+j+1<0 ? 0.f : (*input_signal)->s[i-HISTORY_LENGTH-FORECAST_LENGTH+j+1];
		forward_pass(layers,0);
		backward_pass(layers,&(*input_signal)->s[i]);
		update_weights(layers,learning_rate);
	}

	
	// free input array
	free(inputs);
	
#endif
	dump_weights(layers,7);
	dump_signal(*input_signal);
	return 1;
}
	
void check_predicted_signal (SIGNAL input_signal,SIGNAL output_signal) {
	// allocate three signals to plot
	struct signal mysigs[3];
	
	int num_samples = input_signal->points;

	// allocate the error signal
	float *error = (float *)malloc(sizeof(float)*num_samples);
	
	// empirically find the offset between original signal and predicted signal
	double min_mean_error = 1e50;
	double actual_mean_error;
	int min_error_offset = 0;
	int offset_range = FORECAST_LENGTH + HISTORY_LENGTH + 1;
	//logmsg ("offset_range = %d (%d + %d + 1)",offset_range,FORECAST_LENGTH,HISTORY_LENGTH);	
	for (int offset = 0; offset < offset_range; offset++) {
		double total_error = 0.f;
		
		for (int i=0;i<num_samples-offset;i++)
			total_error += fabs(input_signal->s[i+offset] - output_signal->s[i]);
		
		double mean_error = total_error / (double)(num_samples-offset);
		
		if (mean_error < min_mean_error) {
			min_error_offset = offset;
			min_mean_error = mean_error;
		}
		
		if (offset == FORECAST_LENGTH) {
			actual_mean_error = mean_error;
		}
		//printf("%d,%0.5f\n",offset,mean_error);
	}
	
	logmsg("Best prediction offset = %d, mean error = %0f",min_error_offset,min_mean_error);
	
	min_error_offset = FORECAST_LENGTH;
	
	logmsg("Actual prediction offset = %d, mean error = %0f",min_error_offset,actual_mean_error);
	
	// set error signal
	for (int i=0;i<num_samples-FORECAST_LENGTH;i++)
		error[i] = fabs(input_signal->s[i+min_error_offset] - output_signal->s[i]);
		//error[i] = (*input_signal)->s[i+min_error_offset] - (*output_signal_expected)->s[i];
	
	// zero pad the end of the error array, since the future values of the input signal are unknown
	for (int i=num_samples-min_error_offset;i<num_samples;i++)
		error[i] = 0.f;
	
	// copy the existing input and output signals to the new signal structures
	memcpy((void *)&mysigs[0],(void *)input_signal,sizeof(struct signal));
	memcpy((void *)&mysigs[1],(void *)output_signal,sizeof(struct signal));
	
	// point to the error signal array in the error signal structure
	mysigs[2].s = error;
	mysigs[2].t = mysigs[0].t;
	strcpy(mysigs[2].name,"error");
	
	// plot input, output, and error signals
	FILE *dump_file = fopen("signals.txt","w+");
	if (!dump_file) {
		perror("Error opening \"signals.txt\" for write\n");
		exit(1);
	}
	plot(mysigs,"predicted signal",3,input_signal->t[num_samples-1],dump_file);
	fclose(dump_file);

	// deallocate the error signal array
	free(error);
}

void gen_testbench (FILE *myFile,SIGNAL input_signal,SIGNAL output_signal_expected) {
	fprintf (myFile,"#include <stdio.h>\n"
					"#include <hls_stream.h>\n"
					"#include <ap_fixed.h>\n"
					"#include \"network.h\"\n\n");
	fprintf (myFile,"int main() {\n");
	
	// create input and output
	fprintf (myFile,
			"\thls::stream<%s> input0;\n"
			"\thls::stream<%s> output0;\n\n",DATATYPE,DATATYPE);
			
	// create input signal
	fprintf (myFile,
			 "\t%s input_data[] = {",DATATYPE);
	
	for (int i=0;i<input_signal->points;i++) {
		if (i) fprintf(myFile,",");
		fprintf(myFile,"%0.8f",input_signal->s[i]);
	}
	
	fprintf (myFile,"};\n");
	
	// create output signal (expected)
	fprintf (myFile,
			 "\t%s output_expected[] = {",DATATYPE);
	
	for (int i=0;i<input_signal->points;i++) {
		if (i) fprintf(myFile,",");
		fprintf(myFile,"%0.8f",output_signal_expected->s[i]);
	}
	
	fprintf (myFile,"};\n\n");
	
	// open the output file
	fprintf(myFile,"\tFILE *myFile = fopen(\"output_signal.txt\",\"w+\");\n"
				   "\tif (!myFile) {\n"
				   "\t\tchar str[1024];\n"
				   "\t\tperror(\"Error opening \\\"output_signal.txt\\\" for writing\");\n"
				   "\t\texit(1);\n"
				   "\t}\n\n");
	
	// prime the fifo
	fprintf(myFile,"\tfor (int i=0;i<%d;i++) {\n"
				   "\t\tinput0.write(input_data[i]);\n"
				   "\t\tmynetwork (input0,output0);\n"
				   "\t\tfprintf(myFile,\"%%0.8e,%%0.8e,%%0.8e\\n\",input_data[i],output_expected[i],output0.read());\n"
				   "\t}\n\n",input_signal->points);
	
	fprintf(myFile,"\tfclose(myFile);\n\n"
				   "return 0;\n"
				   "}\n");
}

void compile_testbench_file (const char *filename,void **dl_handle,void (**fn)(hls::stream<float>&,hls::stream<float>&)) {
	char str[1024],object_name[1024],shared_object_name[1024],shared_object_path[1024];
	int ret;

	// assume the filename has a ".cpp" extension
	sscanf(filename,"%[^.]",object_name);
	strcat(object_name,".o");
	sscanf(filename,"%[^.]",shared_object_name);
	strcat(shared_object_name,".so");
	snprintf(shared_object_path,1024,"./%s",shared_object_name);

	// compile the library file
	snprintf(str,1024,"%s -I include -c -g -fpic -o %s %s 2> compile.log",COMPILE_COMMAND,object_name,filename);
	logmsg("Compiling HLS network model using \"%s\"",str);
	FILE *myFile = popen(str,"r");
	if (!myFile) {
		snprintf(str,1024,"ERROR compiling \"%s",filename);
		perror(str);
		exit(1);
	}
	ret = pclose(myFile);
	if (ret != 0) {
		fprintf(stderr,"[ERROR] Compile of DUT model failed, see compile.log\n");
		exit(1);
	}
	
	// convert the library file to a shared object
	snprintf(str,1024,"%s -shared -o %s %s 2> compile.log",COMPILE_COMMAND,shared_object_name,object_name);
	logmsg("Converting HLS network model to shared object using \"%s\"",str);
	myFile = popen(str,"r");
	if (!myFile) {
		snprintf(str,1024,"ERROR compiling \"%s",filename);
		perror(str);
		exit(1);
	}
	ret = pclose(myFile);
	if (ret != 0) {
		fprintf(stderr,"[ERROR] Compile of shared object failed, see compile.log\n");
		exit(1);
	}
	
	logmsg("Loading HLS network model into memory");
	char *error;
	*dl_handle = dlopen(shared_object_path, RTLD_LAZY);
	if (!*dl_handle) { 
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	*fn = (void (*)(hls::stream<float>&,hls::stream<float>&))dlsym(*dl_handle, "mynetwork_dut");
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(1);
	}
	//(*fn)();
	//dlclose(lib_handle);
	return;
}

void get_results_from_dut(SIGNAL input_signal,
			  SIGNAL output_signal_expected,
			  SIGNAL output_signal_dut,
			  void (*fn)(hls::stream<float>&,hls::stream<float> &)) {

	hls::stream<float> input0,output0;

	for (int i=0;i<input_signal->points;i++) {
		input0.write(input_signal->s[i]);
		fn(input0,output0);
		output_signal_dut->t[i]=input_signal->t[i];
		output_signal_dut->s[i]=output0.read();
	}
	
	struct signal signals[3];
	//memcpy((void *)&signals[0],(void *)input_signal,sizeof(struct signal));
	//memcpy((void *)&signals[1],(void *)output_signal_expected,sizeof(struct signal));
	memcpy((void *)&signals[0],(void *)output_signal_dut,sizeof(struct signal));
	plot(signals,"DUT",1,signals[0].t[signals[0].points-1],0);
}

void validate_test_bench (const char *source_name,SIGNAL input_signal,SIGNAL output_signal_expected) {
	// compile the HLS code into a dynamically loadable shared object
	void *lib_handle;
	void (*dut)(hls::stream<float>&,hls::stream<float>&);
	compile_testbench_file (source_name,&lib_handle,&dut);

	// allocate space for the output of the DUT
	SIGNAL output_signal_dut = (SIGNAL)malloc(sizeof(struct signal));
	output_signal_dut->points = output_signal_expected->points;
	output_signal_dut->sample_rate = output_signal_expected->sample_rate;
	output_signal_dut->t = (float *)malloc(sizeof(float)*output_signal_dut->points);
	output_signal_dut->s = (float *)malloc(sizeof(float)*output_signal_dut->points);
	strcpy(output_signal_dut->name,"DUT output");
	// get the results from the DUT
	get_results_from_dut (input_signal,output_signal_expected,output_signal_dut,dut);
	check_predicted_signal(input_signal,output_signal_dut);
}
