#include <stdio.h>
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {

	int s = 3;
	int x, y, z, i;

	int test[s][s][s]; //{{{0,1,2},{3,4,5},{6,7,8}},{{9,10,11},{12,13,14},{15,16,17}}, {{18,19,20}, {21,22,23}, {24,25,26}}};

	int test2[27*3];// = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26};

	for (i=0; i<27*3; i++) 
		test2[i] = i;


{
	for (z = 0; z<s; z++)
		for (y=0; y<s; y++)
			{
				for (x=0; x<s; x++)
				{
					test[z][y][x]= test2[1+(x+y*s+z*s*s)*3];
					cout <<  test[z][y][x] << endl;
				}
			}
}

	//cout << test[2][2][2] << endl;

	return 0;
}
