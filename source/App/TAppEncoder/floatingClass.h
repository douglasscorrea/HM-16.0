/* 
 * File:   floatingClass.h
 * Author: iagostorch
 *
 * Created on August 4, 2015, 4:10 PM
 */

#ifndef FLOATINGCLASS_H
#define	FLOATINGCLASS_H

//#include <cstring>
//#include <string>
#include "../../Lib/TLibCommon/TypeDef.h"

using namespace std;
        
class floatingClass
{
    
public:
    int maxTlayer;
    int testEncTop;
    int testGOP;
    Int adaptiveTilesFlag;
    Int* sumLCUPartitions;
    Int m_iGopSize;
    Int refreshTiling;
    UInt m_totalCoded;
    UInt frameHeightInCU;
    UInt frameWidthInCU;
    string firstFrameTilingMethod;
    string tilingMethod;


    int getMaxTlayer () { return maxTlayer; }
    int getTestEncTop () { return testEncTop; }
    int getTestGOP () { return testGOP; }
    int getGOPSize () {return m_iGopSize; } 

};

#endif	/* FLOATINGCLASS_H */

