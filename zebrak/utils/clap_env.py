#!/usr/bin/env python3
rep = 3
bit = 128//8
for i in range(128):
    k = i//bit
    if k < rep:
        len = bit
        pos = i - k*bit
    else:
        len = (8 - rep)*bit
        pos = i - rep*bit
    #print(i, pos, len)
    print(' '+str(int((1-pos/len)**(1.6)*255)) + ',')
