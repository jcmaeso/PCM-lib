/*
 * wave.h
 *
 *  Created on: 25 mar. 2018
 *      Author: jcala
 */

#ifndef WAV_API_H_
#define WAV_API_H_

enum WAV_ERROR_CODE{
	FILE_NOT_FOUND = 1,
	NOT_PCM_FORMAT = 2,
	SAMPLE_SIZE_ERROR = 3,
	HEADER_READ_ERROR = 4,
	OUTPUT_TEST_FILE_ERROR = 5
};

// WAVE file header format
typedef struct WAV_HEADER {
	unsigned char riff[4];						// RIFF string
	unsigned int overall_size	;				// overall size of file in bytes
	unsigned char wave[4];						// WAVE string
	unsigned char fmt_chunk_marker[4];			// fmt string with trailing null char
	unsigned int length_of_fmt;					// length of the format data
	unsigned int format_type;					// format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
	unsigned int channels;						// no.of channels
	unsigned int sample_rate;					// sampling rate (blocks per second)
	unsigned int byterate;						// SampleRate * NumChannels * BitsPerSample/8
	unsigned int block_align;					// NumChannels * BitsPerSample/8
	unsigned int bits_per_sample;				// bits per sample, 8- 8bits, 16- 16 bits etc
	unsigned char data_chunk_header [4];		// DATA string or FLLR string
	unsigned int data_size;						// NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
}WAV_HEADER;

typedef struct WAV_SAMPLES{
	int bits_resolution;
	long num_samples;
	long buffer_sample_size;
	long current_sample_read;
	long bytes_in_each_channel;
	long size_of_each_sample;
	float duration_seconds;
	int** channels;
}WAV_SAMPLES;

typedef struct AUDIO_FILE{
	WAV_HEADER* header;
	WAV_SAMPLES* samples;
	FILE* file_flux;
}AUDIO_FILE;

AUDIO_FILE* new_audio_file(const char* filename);
int read_pcm_file(AUDIO_FILE* audio);
int init_pcm_file(AUDIO_FILE* audio,long buffer_sample_size);
void delete_pcm_file(AUDIO_FILE* audio);

#endif /* WAV_API_H_ */
