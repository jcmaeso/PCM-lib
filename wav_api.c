/*
 * wave.c
 *
 *  Created on: 26 mar. 2018
 *      Author: jcala
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "wav_api.h"
#define OUTFILE

#ifdef OUTFILE
typedef struct{
	long low_limit;
	long high_limit;
	int minus_factor;
}Limits;
#endif

void init_sample_info(WAV_HEADER* header,WAV_SAMPLES* samples,long buffer_sample_size);
void init_audio_data(WAV_HEADER* header, WAV_SAMPLES* samples, AUDIO_FILE* audio);
int read_pcm_header(FILE* audiofile, WAV_HEADER* header);
int read_pcm_channel(WAV_SAMPLES* samples, uint8_t* sample_buffer,long channel,long index);
int read_pcm_samples(FILE* audiofile, WAV_HEADER* header, WAV_SAMPLES* samples);
void clear_audio_channel_buffs(AUDIO_FILE* audio);
int conver_32bits_little2big(uint8_t* buffer_32bits);
int conver_16bits_little2big(uint8_t* buffer_16bits);
float calc_duration_seconds(WAV_HEADER* header);
long calc_bytes_in_channel(WAV_HEADER* header);
long calc_size_of_sample(WAV_HEADER* header);
long calc_num_samples(WAV_HEADER* header);
char* conver_format_type2name(int format_type);
void print_header_data(WAV_HEADER* header);
void print_samples_data(WAV_SAMPLES* samples);
void read_error_check(int read);
char* seconds_to_time(float raw_seconds);

#ifdef OUTFILE
void load_limits(Limits* num_limits,int bits_per_sample);
void write_test_file(FILE* out,int current_data,Limits* num_limits);
#endif


int read_pcm_file_samples(AUDIO_FILE* audio){
	long  i,num_samples;
	int current_channel,current_data,end;
	uint8_t* sample_buffer;

	sample_buffer = (uint8_t*)malloc(sizeof(uint8_t)* audio->samples->size_of_each_sample);
	num_samples = audio->samples->buffer_sample_size;
	end = 0;

	if(audio->samples->current_sample_read + audio->samples->buffer_sample_size > audio->samples->num_samples){
		num_samples = audio->samples->num_samples - audio->samples->current_sample_read;
		end = 1;
	}

	clear_audio_channel_buffs(audio);
	for(i = 0; i < num_samples; i++){
		if(fread(sample_buffer,audio->samples->size_of_each_sample,1, audio->file_flux)){
			for(current_channel = 0;  current_channel < audio->header->channels; current_channel++){
				current_data = read_pcm_channel(audio->samples,sample_buffer,current_channel,i);
			}
		}
	}

	audio->samples->current_sample_read += num_samples;
	return end;
}

void clear_audio_channel_buffs(AUDIO_FILE* audio){
	uint8_t* sample_buffer;
	long i = 0;
	int current_channel;
	sample_buffer = (uint8_t*)malloc(sizeof(uint8_t)*audio->samples->size_of_each_sample);
	for(i = 0; i < audio->samples->size_of_each_sample;i++){
		*(sample_buffer+i) = 0x00;
	}
	for(i = 0; i < audio->samples->buffer_sample_size;i++){
		for(current_channel = 0;  current_channel < audio->header->channels; current_channel++){
			read_pcm_channel(audio->samples,sample_buffer,current_channel,i);
		}
	}
}
/**
 * @brief
 * Carga en la estructura AUDIO_FILE todo el contenido de un fichero PCM
 * Si el fichero no fuese PCM devuelve error
 *
 * @param param1 Ruta al fichero pcm.
 * @param param2 Estructura de audio.
 * @return WAV_ERROR_CODE si hay error, 0 si no hay problema
 * @note Something to note.
 * @warning Warning.
 */
int read_pcm_file(AUDIO_FILE* audio){
	#ifdef OUTFILE
		FILE* out;
		Limits num_limits;
	#endif
	uint8_t* sample_buffer;
	long i;
	unsigned int current_channel;
	int current_data;
	int error_code;


	#ifdef OUTFILE
	out = fopen("test.txt", "wb");
	if(!out){
		fprintf(stderr,"Error al crear output test");
		return OUTPUT_TEST_FILE_ERROR;
	}
	#endif
	error_code = init_pcm_file(audio,0l);
	if(error_code){
		return error_code;
	}
	/*Stdout print info for debugging*/
	print_samples_data(audio->samples);
	print_header_data(audio->header);

	/*Inicializacion del buffer de lectura temporal*/
	sample_buffer = (uint8_t*)malloc(sizeof(uint8_t)* audio->samples->size_of_each_sample);
#ifdef OUTFILE
	load_limits(&num_limits,audio->header->bits_per_sample);
#endif
	/*Lectura de muestras del fichero PCM*/
	for(i = 0; i < audio->samples->num_samples; i++){
		if(fread(sample_buffer,audio->samples->size_of_each_sample,1, audio->file_flux)){
			for(current_channel = 0;  current_channel < audio->header->channels; current_channel++){
				current_data = read_pcm_channel(audio->samples,sample_buffer,current_channel,i);
				#ifdef OUTFILE
				if(current_channel == 0)
					write_test_file(out,current_data,&num_limits);
				#endif
			}
		}
	}
	/*Liberamos recursos*/
	free(sample_buffer);
#ifdef OUTFILE
	fclose(out);
#endif
	return 0;
}


AUDIO_FILE* new_audio_file(const char* filename){
	AUDIO_FILE* audio = (AUDIO_FILE*)malloc(sizeof(AUDIO_FILE));
	audio->file_flux = fopen(filename, "rb");
	if(!audio->file_flux){
		fprintf(stderr,"Error abriendo el fichero especificado");
		free(audio);
		return NULL;
	}
	audio->header = (WAV_HEADER*)malloc(sizeof(WAV_HEADER));
	audio->samples = (WAV_SAMPLES*)malloc(sizeof(WAV_SAMPLES));
	return audio;
}

int init_pcm_file(AUDIO_FILE* audio,long buffer_sample_size){
	int error_code;
	error_code = read_pcm_header(audio->file_flux,audio->header);
	/*Comprovacion de errores en la lectura de la cabecera*/
	if(error_code){
		return error_code;
	}
	/*Comprovacion del tipo de fichero wav*/
	if(audio->header->format_type != 1){
		fprintf(stderr,"Error, el fichero de audio no es PCM \n");
		fclose(audio->file_flux);
		return NOT_PCM_FORMAT;
	}
	init_sample_info(audio->header,audio->samples,buffer_sample_size);
	return 0;
}

/**
 * @brief
 * Inicializa los campos de la estructura WAV_SAMPLE con los
 * datos de un WAV_HEADER
 * @param param1 Estructura AUDIO_FILE que se quiere eliminar.
 * @note libera primero la memoria de wav_samples, y luego la del header
 * @warning Si la estructura no esta inicializada puede ser peligroso.
 */
void delete_pcm_file(AUDIO_FILE* audio){
	int i;
	//Borrado de los diferentes canales de audio
	for(i = 0; i < audio->header->channels; i++)
		free(*((audio->samples->channels)+i));
	//Borrado del array de punteros
	free(audio->samples->channels);
	//Borrado del tipo samples
	free(audio->samples);
	//Borrado del header
	free(audio->header);
	//Close
	fclose(audio->file_flux);
	//Borrado de la superestructura
	free(audio);
	/*STDOUT*/
	printf("Se ha liberado correctamente la memoria \n");
}

/**
 * @brief
 * Inicializa los campos de la estructura WAV_SAMPLE con los
 * datos de un WAV_HEADER
 * @param param1 Estructura Header.
 * @param param2 Estructura Sample.
 * @note Something to note.
 * @warning Warning.
 */

void init_sample_info(WAV_HEADER* header,WAV_SAMPLES* samples,long buffer_sample_size){
	int i = 0;
	samples->bytes_in_each_channel = calc_bytes_in_channel(header);
	samples->num_samples = calc_num_samples(header);
	samples->size_of_each_sample = calc_size_of_sample(header);
	samples->duration_seconds = calc_duration_seconds(header);
	samples->channels = (int**)malloc(sizeof(int*)*header->channels);
	samples->current_sample_read = 0;
	samples->buffer_sample_size = buffer_sample_size;
	for(i = 0; i < header->channels; i++){
		if(buffer_sample_size == 0)
			*((samples->channels)+i) = malloc(sizeof(int)*samples->num_samples);
		else
			*((samples->channels)+i) = malloc(sizeof(int)*samples->buffer_sample_size);
	}
}

/**
 * @brief
 * Inicializa los campos de la estructura audiodata con los
 * datos de un WAV_HEADER
 * @param param1 Estructura Header.
 * @param param2 Estructura Sample.
 * @param param3 Estructura audio a enlazar
 * @note Something to note.
 * @warning Warning.
 */

void init_audio_data(WAV_HEADER* header, WAV_SAMPLES* samples, AUDIO_FILE* audio){
	audio->header = header;
	audio->samples = samples;
}

/**
 * @brief
 * Pasa el buff de datos al correspondiente array, en funcion de canal e indice
 * @param param1 Estructura Header.
 * @param param2 Estructura Sample.
 * @note Something to note.
 * @warning Warning.
 */

int read_pcm_channel(WAV_SAMPLES* samples, uint8_t* sample_buffer,long channel,long index){
	int* my_channel = *(samples->channels+channel);
	if (samples->bytes_in_each_channel == 4) {
		*(my_channel+index) = conver_32bits_little2big(sample_buffer);
	} else if (samples->bytes_in_each_channel == 2) {
		*(my_channel+index)  = conver_16bits_little2big(sample_buffer);
	} else if (samples->bytes_in_each_channel == 1) {
		*(my_channel+index)  = sample_buffer[0];
	}

	return *(my_channel+index);
}

/**
 * @brief
 * Inicializa los campos de la estructura WAV_SAMPLE con los
 * datos de un WAV_HEADER
 * @param param1 Flujo hacia fichero de audio.
 * @param param2 Estructura Header.
 * @note Something to note.
 * @warning Warning.
 */


int read_pcm_header(FILE* audiofile, WAV_HEADER* header){

	uint8_t buffer_32bits[4];
	uint8_t buffer_16bits[2];
	int read;
	//Esto lee el campo Riff de la cabecera del .wav
	read = fread(header->riff,sizeof(header->riff), 1, audiofile);
	read_error_check(read);
	//Lee tamano en el fichero es little endian
	read = fread(buffer_32bits, sizeof(buffer_32bits), 1, audiofile);
	header->overall_size = conver_32bits_little2big(buffer_32bits);
	//Lee el campo wav
	read = fread(header->wave, sizeof(header->wave), 1, audiofile);
	//Lee campo chunk master
	read = fread(header->fmt_chunk_marker, sizeof(header->fmt_chunk_marker), 1, audiofile);
	//Lee informacion muestras
	read = fread(buffer_32bits, sizeof(buffer_32bits), 1, audiofile);
	header->length_of_fmt = conver_32bits_little2big(buffer_32bits);
	//Lee informacion de formato
	read = fread(buffer_16bits, sizeof(buffer_16bits), 1, audiofile);
	header->format_type = conver_16bits_little2big(buffer_16bits);
	//Lee numero de canales
	read = fread(buffer_16bits, sizeof(buffer_16bits), 1, audiofile);
	header->channels = conver_16bits_little2big(buffer_16bits);
	//Lee muestreo
	read = fread(buffer_32bits, sizeof(buffer_32bits), 1, audiofile);
	header->sample_rate = conver_32bits_little2big(buffer_32bits);
	//Lee el byterate
	read = fread(buffer_32bits, sizeof(buffer_32bits), 1, audiofile);
	header->byterate = conver_32bits_little2big(buffer_32bits);
	//Lee Block align
	read = fread(buffer_16bits, sizeof(buffer_16bits), 1, audiofile);
	header->block_align = conver_16bits_little2big(buffer_16bits);
	//Lee bits por muestra
	read = fread(buffer_16bits,sizeof(buffer_16bits), 1, audiofile);
	header->bits_per_sample = conver_16bits_little2big(buffer_16bits);
	//Lee data marker
	read = fread (header->data_chunk_header,sizeof(header->data_chunk_header), 1, audiofile);
	//Lee data size
	read = fread(buffer_32bits,sizeof(buffer_32bits), 1, audiofile);
	header->data_size = conver_32bits_little2big(buffer_32bits);

	return 0;
}


/**
 * @brief
 * Convierte 32bits little endian en 32 buts big endian
 * @param param1 buffer con los 32 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
int conver_32bits_little2big(uint8_t* buffer_32bits){
	return buffer_32bits[0] | (buffer_32bits[1] << 8) | (buffer_32bits[2] << 16)| (buffer_32bits[3] << 24);
}
/**
 * @brief
 * Convierte 16bits little endian en 16 bits big endian
 * @param param1 buffer con los 16 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
int conver_16bits_little2big(uint8_t* buffer_16bits){
	return buffer_16bits[0] | (buffer_16bits[1] << 8);
}
/**
 * @brief
 * Convierte 16bits little endian en 16 bits big endian
 * @param param1 buffer con los 16 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
float calc_duration_seconds(WAV_HEADER* header){
	return (float) header->overall_size / header->byterate;
}
/**
 * @brief
 * Convierte 16bits little endian en 16 bits big endian
 * @param param1 buffer con los 16 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
long calc_bytes_in_channel(WAV_HEADER* header){
	return calc_size_of_sample(header)/header->channels;
}
/**
 * @brief
 * Convierte 16bits little endian en 16 bits big endian
 * @param param1 buffer con los 16 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
long calc_size_of_sample(WAV_HEADER* header){
	return (header->channels * header->bits_per_sample) / 8;
}
/**
 * @brief
 * Convierte 16bits little endian en 16 bits big endian
 * @param param1 buffer con los 16 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
long calc_num_samples(WAV_HEADER* header){
	return (8 * header->data_size)/ (header->channels * header->bits_per_sample);
}
/**
 * @brief
 * Convierte 16bits little endian en 16 bits big endian
 * @param param1 buffer con los 16 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
char* conver_format_type2name(int format_type){
	if (format_type == 1)
		return "PCM";
	else if (format_type == 6)
		return "A-law";
	else if (format_type == 7)
		return "Mu-law";
	return "UNKWOWN";
}
/**
 * @brief
 * Imprime los datos del header po
 *  * @param param1 buffer con los 16 bits de datos a convertir
 * @note Something to note.
 * @warning Warning.
 */
void print_header_data(WAV_HEADER* header){
	printf("(1-4): %s \n", header->riff);
	printf("(5-8) Overall size: bytes:%u, Kb:%u \n", header->overall_size,
			header->overall_size / 1024);
	printf("(9-12) Wave marker: %s\n", header->wave);
	printf("(13-16) Fmt marker: %s\n", header->fmt_chunk_marker);
	printf("(17-20) Length of Fmt header: %u \n", header->length_of_fmt);
	printf("(21-22) Format type: %u %s \n", header->format_type, conver_format_type2name(header->format_type));
	printf("(23-24) Channels: %u \n", header->channels);
	printf("(25-28) Sample rate: %u\n", header->sample_rate);
	printf("(29-32) Byte Rate: %u , Bit Rate:%u\n", header->byterate,
			header->byterate * 8);
	printf("(33-34) Block Alignment: %u \n", header->block_align);
	printf("(35-36) Bits per sample: %u \n", header->bits_per_sample);
	printf("(37-40) Data Marker: %s \n", header->data_chunk_header);
	printf("(41-44) Size of data chunk: %u \n", header->data_size);
}
/**
 * @brief
 * Imprime por pantalla los datos de las muestras de samples
 * @param Cabecera con los datos acordes
 * @note Something to note.
 * @warning Warning.
 */
void print_samples_data(WAV_SAMPLES* samples){
	printf("Number of samples:%lu \n", samples->num_samples);
	printf("Size of each sample:%ld bytes\n", samples->size_of_each_sample);
	printf("Approx.Duration in seconds=%f\n", samples->duration_seconds);
	printf("Approx.Duration in h:m:s=%s\n",seconds_to_time(samples->duration_seconds));
}
/**
 * @brief
 * Funcion que checkea error de lectura de fichero
 * @param parametro de vuelta de fread
 * @note Something to note.
 * @warning Warning.
 */
void read_error_check(int read){
	if(read <= 0){
		fprintf(stderr,"Error leyendo fichero");
	}
}

//TODO: CAMBIAR NUMEROS POR LIMITES
void load_limits(Limits* num_limits,int bits_per_sample){
	switch (bits_per_sample) {
	case 8:
		num_limits->low_limit = SCHAR_MIN;
		num_limits->high_limit = SCHAR_MAX;
		num_limits->minus_factor = UCHAR_MAX;
		break;
	case 16:
		num_limits->low_limit = SHRT_MIN;
		num_limits->high_limit = SHRT_MAX;
		num_limits->minus_factor = USHRT_MAX;
		break;
	case 32:
		num_limits->low_limit = INT_MIN;
		num_limits->high_limit = INT_MAX;
		num_limits->minus_factor = UINT_MAX;
		break;
	}
}

void write_test_file(FILE* out,int current_data,Limits* num_limits){
	if(current_data > num_limits->high_limit){
		current_data -= num_limits->minus_factor;
	}
	fprintf(out,"%d \n",current_data);
}




char* seconds_to_time(float raw_seconds){
	char *hms;
	int hours, hours_residue, minutes, seconds, milliseconds;
	hms = (char*) malloc(100);

	sprintf(hms, "%f", raw_seconds);

	hours = (int) raw_seconds / 3600;
	hours_residue = (int) raw_seconds % 3600;
	minutes = hours_residue / 60;
	seconds = hours_residue % 60;
	milliseconds = 0;

	// get the decimal part of raw_seconds to get milliseconds
	char *pos;
	pos = strchr(hms, '.');
	int ipos = (int) (pos - hms);
	char decimalpart[15];
	memset(decimalpart, ' ', sizeof(decimalpart));
	strncpy(decimalpart, &hms[ipos + 1], 3);
	milliseconds = atoi(decimalpart);

	sprintf(hms, "%d:%d:%d.%d", hours, minutes, seconds, milliseconds);
	return hms;
}
