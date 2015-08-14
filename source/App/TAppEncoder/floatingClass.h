/* 
 * File:   floatingClass.h
 * Author: iagostorch
 *
 * Created on August 4, 2015, 4:10 PM
 */

#ifndef FLOATINGCLASS_H
#define	FLOATINGCLASS_H

class floatingClass
{
    
public:
    int maxTlayer;
    int testEncTop;
    int testGOP;
    int GOPSize;
    int getMaxTlayer () { return maxTlayer; }
    int getTestEncTop () { return testEncTop; }
    int getTestGOP () { return testGOP; }
    int getGOPSize () {return GOPSize; } 

};

#endif	/* FLOATINGCLASS_H */

