#!/usr/bin/python
import numpy
from numpy import array


def puta(a, b, X,Y,Z):

	for z in xrange(Z):
		for y in xrange(Y):
			for x in xrange(X):
				#print z, y, x	
				a[z, y, x] = b[x+y*Y+z*Z*Y]
	
	return a				


def getInter(a, b, X, Y, Z):

	Z = 1
	Y = 1
	X = 12	
	for z in xrange(Z):
		for y in xrange(Y):
			for x in xrange(0, X, 3):
				print b[x+y*Y+z*Z*Y],
	



x, y, z = 3, 3, 3

a = array(0)
a = numpy.resize(a, (x, y, z))
print "Empty:"
print a

b = array(range(x*y*z+10))
#print b

c = numpy.resize(b, (x,y,z))
#print c

print "Full: "
a = puta(a, b, 3, 3, 3)
print a

c = getInter(a, b, x, y, z)
