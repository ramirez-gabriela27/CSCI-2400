#include "Filter.h"
#include <iostream>

Filter::Filter(int _dim)
{
  divisor = 1;
  dim = _dim;
  data = new int[dim * dim];
}

// int Filter::get(int r, int c)
// {
//   return data[ r * dim + c ];
// }

// void Filter::set(int r, int c, int value)
// {
//   data[ r * dim + c ] = value;
// }

// int Filter::getDivisor()
// {
//   return divisor;
// }

// void Filter::setDivisor(int value)
// {
//   divisor = value;
// }

// int Filter::getSize()
// {
//   return dim;
// }

// void Filter::info()
// {
//   cout << "Filter is.." << endl;
//     //Switching rows and column loops, optimizing use of DRAM cache
//   for (int row = 0; row < dim; row++) {
//     for (int col = 0; col < dim; col++) {
//       cout << get(row,col) << " ";
//     }
//     cout << endl;
//   }
// }
