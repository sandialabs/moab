from pymoab import core
from pymoab import types
from pymoab.hcoord import HomCoord
from math import sqrt
import numpy as np
from driver import test_driver


def test_homcoord():


    #try default construction
    h = HomCoord()
    #now with some sample args
    h = HomCoord([1,1,1])
    h = HomCoord([1,1,1,1])

    #now a case that should fail
    try:
        h = HomCoord([1])
    except:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError

    h = HomCoord([1,2,3,4])
    assert(1 == h.i())
    assert(2 == h.j())
    assert(3 == h.k())
    assert(4 == h.h())
    assert(14 == h.length_squared())
    assert(int(sqrt(14)) == h.length())
    h.normalize()
    assert(1 == h.length())

    h.set(4,3,2,1)
    assert(4 == h.i())
    assert(3 == h.j())
    assert(2 == h.k())
    assert(1 == h.h())

    # testing for possible bug in iterator
    #these should work
    assert(4 == h[0])
    assert(3 == h[1])
    assert(2 == h[2])
    assert(1 == h[3])
    try:
        h[4]
    except:
        pass
    else:
        print "Shouldn't be here. Test fails"
        raise AssertionError
    
if __name__ == "__main__":
    tests = [test_homcoord,]
    test_driver(tests)
