#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "Filter.h"

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

class Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double applyFilter(class Filter *filter, cs1300bmp *input, cs1300bmp *output){
    long long cycStart, cycStop;

    cycStart = rdtscll();

    output -> width = input -> width;
    output -> height = input -> height;
    
    //Elimination of unneeded memory references
    int inWidth = input->width-1;
    int inHeight = input->height-1;
    //Elimination of function call
    int filterSize = filter->getSize();
    int filterDivisor = filter->getDivisor();
    
    //Switching rows and column loops, optimizing use of DRAM cache
    for(int row = 1; row < inHeight; row++) {
        for(int col = 1; col < inWidth; col++) {

        //loop unrolling
        //Eliminating more unneeded memory references
        short temp1 = 0;
        short temp2 = 0;
        short temp3 = 0;

        //same as loop above
        for (int i = 0; i < filterSize; i++) {
            for (int j = 0; j < filterSize; j++) {
                //Elemination of function call
                int filterGet = filter->get(i,j);
                //continuing loop unrolling
                temp1 += input->color[0][row+i-1][col+j-1]*filterGet;
                temp2 += input->color[1][row+i-1][col+j-1]*filterGet;
                temp3 += input->color[2][row+i-1][col+j-1]*filterGet;
            }
        }
            
        temp1 /= filterDivisor;  
        temp2 /= filterDivisor;
        temp3 /= filterDivisor;
            
        //bit operations instead of if/else statements
        temp1 = (temp1 > 255) ? 255: temp1;
        temp1 = (temp1 < 0) ? 0: temp1;
            
        temp2 = (temp2 > 255) ? 255: temp2;
        temp2 = (temp2 < 0) ? 0: temp2;
        
        temp3 = (temp3 > 255) ? 255: temp3;
        temp3 = (temp3 < 0) ? 0: temp3;

        output -> color[0][row][col] = temp1;
        output -> color[1][row][col] = temp2;
        output -> color[2][row][col] = temp3;
        }
    }
    
    cycStop = rdtscll();
    double diff = cycStop - cycStart;
    double diffPerPixel = diff / (output->width * output->height);
    fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
    diff, diff / (output->width * output->height));
    return diffPerPixel;
}
