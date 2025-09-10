#include "data.h"
#include <iostream>
#include <random>
using namespace std;

	




union vectors
{
	char a[sizeof(float)];
	float b;
};

char *float_to_char(float float_array[], int dimention){
	char *char_array;
	char_array = new char[dimention*sizeof(float)];
	union vectors v;
	for (int i = 0 ; i < dimention; i++){
		
		v.b= float_array[i];
		for(int j = 0+4*i , k = 0; j < 4+4*i; j++, k++){
			char_array[j] = v.a[k]; 
		}
	}
	return char_array;
}

float *char_to_float(char char_array[], int dimention){

	float *float_array = new float[dimention];

	union vectors v;

	for (int i = 0 ; i < dimention; i++){
		
		for(int j = 0+4*i , k = 0; j < 4+4*i; j++, k++){
			v.a[k] = char_array[j]; 
		}
		float_array[i] = v.b;
	}
	return float_array;
} 

float **init_emb(int size, int dimention ){
	float **float_2array; 
	float_2array = new float*[size];

	for (int i = 0; i < size; i++){

		float_2array[i] = new float[dimention];
		for (int j = 0; j < dimention; j++){
			float_2array[i][j] = (float) rand() / (RAND_MAX + 1.0);

		} 
	}
	
	return float_2array;
}



char *float1_to_char1(float *float_1array , int dimention){
	char * char_1array;

	char_1array = float_to_char(float_1array,dimention);
	

	delete [] float_1array;
	
	return char_1array;
}

char **float2_to_char2(float **float_2array, int size, int dimention){
	char ** char_2array = new char *[size];
	for (int i = 0; i < size; i++){
		char_2array[i] = float_to_char(float_2array[i],dimention);
	}
	for (int i = 0; i < size; i++){
		delete [] float_2array[i];
	}
	delete [] float_2array;
	
	return char_2array;
}


float **char2_to_float2(char ** char_2array, int size, int dimention){
	float **float_2array; 
	float_2array = new float *[size];
	for (int i = 0; i < size; i++){
		float_2array[i] = char_to_float(char_2array[i],dimention);
	}
	for (int i = 0; i < size; i++){
		delete [] char_2array[i];
	}

	delete [] char_2array;
	return float_2array;
}

char *char2_to_char1(char ** char_2array, int size, int dimention){
	
	char *char_1 = new char[size*dimention*sizeof(float)];
	int count = 0;
	for (int i = 0; i < size; ++i){
	
	    for (int j = 0; j < dimention*sizeof(float); ++j){
		
	        char_1[count++] = char_2array[i][j];
	        
		}
	}

	for (int i = 0; i < size; i++){
		delete [] char_2array[i];
	}

	delete [] char_2array;
	
	return char_1;
}

char **char1_to_char2(char * char_1, int size, int dimention){
	
	char ** char_2array = new char *[size];

	int count = 0;
	for (int i = 0; i < size; ++i){
	    char_2array[i] = new char[dimention*sizeof(float)];
	    for (int j = 0; j < dimention*sizeof(float); ++j){
		
	        char_2array[i][j] = char_1[count++];   
		}
	}

	delete [] char_1;
	
	return char_2array;
}

float *float2_to_float1(float **emb_data,int size, int dimention){
		
	float *float_1 = new float[size*dimention];
	int count = 0;
	for (int i = 0; i < size; ++i){
	
	    for (int j = 0; j < dimention; ++j){
		
	        float_1[count++] = emb_data[i][j];
	        
		}
	}

	
	return float_1;
}

void DATA::transfer_byte(){
	emb_data_char = float2_to_char2(emb_data, m_size, m_dimention);

}

void DATA::initial(){
	emb_data = init_emb( m_size, m_dimention );
}

float **DATA::transfer_float_debug(char** emb){
	return char2_to_float2(emb, m_size, m_dimention);
}
float **DATA::transfer_float(){
	emb_data = char2_to_float2(emb_data_char, m_size, m_dimention);
	return emb_data;
}
void DATA::transfer_char1()		
{ 
	char1 = char2_to_char1(emb_data_char, m_size, m_dimention);
}

void DATA::transfer_char2()		
{ 
	emb_data_char = char1_to_char2(char1, m_size, m_dimention);
}

float **DATA::getdata_float()		
{ 
	return emb_data;
}
char **DATA::getdata_char2()		
{ 
	return emb_data_char;
}

void DATA::transfer_float1()		
{ 
	float1 = float2_to_float1(emb_data, m_size, m_dimention);
}

void DATA::transfer_float1_to_char1()		
{ 
	char1 = float1_to_char1(float1, m_dimention);
}

