/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2014, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TComPicSym.cpp
    \brief    picture symbol class
*/

#include "TComPicSym.h"
#include "TComSampleAdaptiveOffset.h"
#include "TComSlice.h"

//DI BEGIN
#include "math.h"
#include "TComPic.h"

UInt** bitMtx; //Cauane - matrix to store bits per CU
Int firstFrame = 1;
//UInt* previousTilesRowSizes;
std::vector<Int>previousTilesRowSizes;
//UInt* previousTilesColSizes;
std::vector<Int>previousTilesColSizes;
UInt** varianceMap;
UInt** LCUPartMtx;
//DI END

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TComPicSym::TComPicSym()
:m_uiWidthInCU(0)
,m_uiHeightInCU(0)
,m_uiMaxCUWidth(0)
,m_uiMaxCUHeight(0)
,m_uiMinCUWidth(0)
,m_uiMinCUHeight(0)
,m_uhTotalDepth(0)
,m_uiNumPartitions(0)
,m_uiNumPartInWidth(0)
,m_uiNumPartInHeight(0)
,m_uiNumCUsInFrame(0)
,m_apcTComSlice(NULL)
,m_uiNumAllocatedSlice (0)
,m_apcTComDataCU (NULL)
,m_iNumColumnsMinus1 (0)
,m_iNumRowsMinus1(0)
,m_puiCUOrderMap(0)
,m_puiTileIdxMap(NULL)
,m_puiInverseCUOrderMap(NULL)
,m_saoBlkParams(NULL)
{}

//DI BEGIN
UInt** genPartitionsMtx(Int* LCUPartitionsArray, Int heightInCU, Int widthInCU){
    UInt** partMtx;
    Int arrayOffset = 0;
    
    partMtx = new UInt*[heightInCU];
    for(int k=0; k<heightInCU; k++)
        partMtx[k] = new UInt[widthInCU];
    
    for(int i=0; i<heightInCU; i++){
        for(int j=0; j<widthInCU; j++){
            partMtx[i][j] = LCUPartitionsArray[j+arrayOffset];
        }
        arrayOffset += widthInCU;
    }
    
    return partMtx;    
}

//function to extract the original picture luma pixel values - Cauane
//input: the picture itself
//output: a pixel matrix containing all picture pixels
Int** extractLumaPixels(TComPic* pcPic){                        
    UInt stride = pcPic->getStride(COMPONENT_Y);           //pixels are organized in a row vector, this is the step per CU   
    Pel* pxlsPerCu = pcPic->getPicYuvOrg()->getAddr(COMPONENT_Y);     //pixels in a CU
    Int** pixelMtx;
    Int w=0, h=0;
        
    //initialize pixelMtx
    pixelMtx = new Int*[pcPic->getFrameHeightInCU()*64];
    for(int k=0; k<pcPic->getFrameHeightInCU()*64; ++k)
        pixelMtx[k] = new Int[pcPic->getFrameWidthInCU()*64]; 
        
    //access all frame CUs
    for(int l = 0; l < pcPic->getNumCUsInFrame(); l++){
        pxlsPerCu = pcPic->getPicYuvOrg()->getAddr(COMPONENT_Y);
        //iterate inside the 64x64 CU pixels
        for( int y = 0; y < 64; y++ ){
            for( int x = 0; x < 64; x++ ){  
                pixelMtx[y+h][x+w] = pxlsPerCu[x];
                //print the first pixel per CU
                /*
                if(x==0 && y==0){
                    printf("%d-[%dx%d] = %d\n",l,y+h,x+w,pixelMtx[y+h][x+w]);
                    //getchar();
                }
                */
            }
        //increment the step    
        pxlsPerCu += stride;
        }
        
        //assess the pixelMtx index based on the picture height and width
        if(w+64 == pcPic->getPicYuvOrg()->getWidth(COMPONENT_Y)){
            h += 64;
            w = 0;
        }
        else
            w += 64;
    }
    
    //tests if the height of the picture isn't divisible by 64 (copy the last X rows to the rest of the picture) Class B: 1920x1088!
    if(pcPic->getFrameHeightInCU()*64-pcPic->getPicYuvOrg()->getHeight(COMPONENT_Y) > 0){
        //pick the number of rows to be copied
        Int cpRowIdx = pcPic->getFrameHeightInCU()*64-pcPic->getPicYuvOrg()->getHeight(COMPONENT_Y);
        for(int i=pcPic->getPicYuvOrg()->getHeight(COMPONENT_Y)-cpRowIdx; i<pcPic->getPicYuvOrg()->getHeight(COMPONENT_Y); i++){
            for(int j=0; j<pcPic->getPicYuvOrg()->getWidth(COMPONENT_Y); j++){
                pixelMtx[i+cpRowIdx][j] = pixelMtx[i][j];
            }
        }
    }
    
    return pixelMtx;
}

UInt** variancePerLCU(TComPic* pcPic){
    Int** pxMtx;
    Int LCUvector[64*64];
    Int soma = 0;
    Double variance = 0;
    Double** varMap;
    UInt** uintVarMap;
            
    //initialize varianceMap and UIntVarMap
    varMap = new Double*[pcPic->getFrameHeightInCU()];
    uintVarMap = new UInt*[pcPic->getFrameHeightInCU()];
    for(int k=0; k<pcPic->getFrameHeightInCU(); ++k){
        varMap[k] = new Double[pcPic->getFrameWidthInCU()];
        uintVarMap[k] = new UInt[pcPic->getFrameWidthInCU()];
    }
    
    pxMtx = extractLumaPixels(pcPic);
    
    //for to access each LCU of a picture
    for(int h=0; h<pcPic->getFrameHeightInCU()*64; h+=64){
        for(int w=0; w<pcPic->getFrameWidthInCU()*64; w+=64){
            variance = 0;
            soma = 0;
            //for to access the 64x64 pixels inside a LCU
            for(int i=0; i<64; i++){
                for(int j=0; j<64; j++){
                    //vectorizing the pixel matrix
                    LCUvector[(i*64)+j] = pxMtx[i+h][j+w];                
                    //sum of the elements
                    soma = soma + LCUvector[(i*64)+j];
                }
            }
            //calculate variance: each element minus the mean, powered to square            
            for(int k=0; k<(64*64); k++){                
                variance = variance + pow((abs(LCUvector[k])-((Double)soma/(Double)(64*64))),2);
            }                        
            variance = variance / (64*64-1);            
            //store the variance value in its respective LCU address
            varMap[h/64][w/64] = variance;
        }            
    }
    
    
    for(int i=0; i<pcPic->getFrameHeightInCU();i++){        
        for(int j=0; j<pcPic->getFrameWidthInCU();j++){
            uintVarMap[i][j] = round(varMap[i][j]);
        }
    }    
  
    return uintVarMap;
}

//functions used by "qsort" to sort values in descending or ascending order - Cauane
int sortDescending(const void * a, const void * b){
    return (*(int*)b - *(int*)a);
}

int sortAscending(const void * a, const void * b){
    return (*(int*)a - *(int*)b);
}

//function used by "qsort" to sort values in descending, when the array is Double - Cauane
int sortDescendingDoubles (const void *a, const void *b){
   const double *da = (const double *) a;
   const double *db = (const double *) b;

   return (*da < *db)-(*da > *db);
}

//extract row Tile boundaries indexes - Cauane
//input: CU map to apply the partitioning, number of tiles per row, picture height and width in CUs
//output: array containing the size of each tile row
std::vector<Int> getRowTileBoundary(UInt** CUMap, UInt tilesPerRow, Int heightInCU, Int widthInCU)
{
    Int rowDiffMap[heightInCU-1][widthInCU];
    Int soma[heightInCU-1];
    Double var[heightInCU-1];
    Double originalVariance[heightInCU-1];
    //UInt* tileBoundaryIdx = new UInt[tilesPerRow];
    std::vector<Int>tileBoundaryIdx;
    Int boundariesPicked = 0;
    Int tileTotalSize = 0;
    Bool pickPreviousPattern = false;
        
    for(int i=0; i<heightInCU-1; i++){
        //printf("\n");
        for(int j=0; j<widthInCU; j++){
            rowDiffMap[i][j] = CUMap[i][j]-CUMap[i+1][j];
            //printf("%d ", abs(rowDiffMap[i][j]));
        }
    }    
    
    //calculate variance
    //calculate sum along rows
    //printf("\n Soma: \n");
    for(int i=0; i<heightInCU-1; i++){
        soma[i] = 0;
        for(int j=0; j<widthInCU; j++){
            soma[i] = soma[i] + abs(rowDiffMap[i][j]);            
        }
        //printf("%d\n", soma[i]);
    }
    
    //calculate variance: each element minus the mean, powered to square
    //printf("\n\nVariance (pre-sorted): \n");
    for(int i=0; i<heightInCU-1; i++){
        var[i]=0;
        for(int j=0; j<widthInCU; j++){
            var[i] = var[i] + pow((abs(rowDiffMap[i][j])-((Double)soma[i]/(Double)widthInCU)),2);            
        }
        var[i] = var[i] / (widthInCU-1);
        //printf("[%d] - %f\n", i+1, var[i]);
    }
    
    //store the original variance values before sorting
    memcpy(originalVariance, var, sizeof(var));
    
    //sort variance array, which becomes a variance ranking
    qsort(&var[0],sizeof(var)/sizeof(Double),sizeof(Double),sortDescendingDoubles);
    
    //print variance array after being sorted
    
    /*//printf("\nVariance (sorted): \n");
    for(int i=0; i<heightInCU-1; i++){
        printf("%d - %f \t %f\n", i+1, originalVariance[i], var[i]);
    }
    printf("\n");
    */  
    
    //pick the "tilesPerRow" first tile boundary indexes
    while(boundariesPicked != tilesPerRow){                                     //tests the number of boundaries picked
        if(pickPreviousPattern){                                                //if the last pattern has to be picked
            tileBoundaryIdx[0] = 0;                                             //mark the first position to be 0
            return tileBoundaryIdx;
        }
        for(int i=0; i<sizeof(var)/sizeof(Double);i++){
            if(boundariesPicked == tilesPerRow)
                break;
            for(int j=0; j<sizeof(originalVariance)/sizeof(Double); j++){
                if(var[i] == originalVariance[j]){                              //pick the first boundary indexes                 
                    if(boundariesPicked == 0){                                  //if it is the first boundary to be picked,
                        tileBoundaryIdx[boundariesPicked] = j+1;                //just pick the first index                  
                        tileTotalSize = tileBoundaryIdx[boundariesPicked];
                        boundariesPicked++;                                     //increment the number of boundaries and the Tiles' total size
                    }
                    else{
                        if(((tileTotalSize+j+1) > heightInCU) || ((tileTotalSize+j+1) == heightInCU && (boundariesPicked+1) != tilesPerRow)){
                            break;                                              //tests next variance if the following boundaries do not respect the picture limits                            
                        }                                                       //or are equal to the picture total size, and all the boundaries have not been picked
                        else{                            
                            tileBoundaryIdx[boundariesPicked] = j+1;            //pick the index                            
                            qsort(&tileBoundaryIdx[0],boundariesPicked+1,sizeof(Int),sortAscending); //sort in ascending order to the define all sizes in another array                            
                            for(int k=0; k<=boundariesPicked; k++){
                                if(tileBoundaryIdx[k] > tileTotalSize)          //update the Tiles' total size if necessary
                                    tileTotalSize = tileBoundaryIdx[k];
                            }                                                                      
                            boundariesPicked++;                                 //increment the number of boundaries and the Tiles' total size
                        }
                    }
                }
            }
            if(i == sizeof(var)/sizeof(Double) && boundariesPicked != tilesPerRow){ //if the variance ranking is not suitable to define
                pickPreviousPattern = true;                                         //a Tile pattern, make the flag true to pick the last valid
            }
        }
    }
    
    
    //UInt* finalTileArray = new UInt[tilesPerRow];                               //array to store Tiles' sizes
    std::vector<Int>finalTileArray;
    finalTileArray[0] = tileBoundaryIdx[0];
    
    for(int i=1; i<boundariesPicked; i++){        
        finalTileArray[i] = tileBoundaryIdx[i]-tileBoundaryIdx[i-1];            //calculus of each size
    }
    
    return finalTileArray;
}

//extract column Tile boundaries indexes - Cauane
//input: CU map to apply the partitioning, number of tiles per column, picture height and width in CUs
//output: array containing the size of each tile column
std::vector<Int> getColTileBoundary(UInt** CUMap, UInt tilesPerCol, Int heightInCU, Int widthInCU)
{
    Int colDiffMap[heightInCU][widthInCU-1];
    Int soma[widthInCU-1];
    Double var[widthInCU-1];
    Double originalVariance[widthInCU-1];
    //UInt* tileBoundaryIdx = new UInt[tilesPerCol];
    std::vector<Int> tileBoundaryIdx;
    Int boundariesPicked = 0;
    Bool tileMinSize = true;
    Int tileTotalSize = 0;
    Bool pickPreviousPattern = false;
        
    for(int i=0; i<heightInCU; i++){
        //printf("\n");
        for(int j=0; j<widthInCU-1; j++){
            colDiffMap[i][j] = CUMap[i][j]-CUMap[i][j+1];
            //printf("%d ", abs(colDiffMap[i][j]));
        }
    }    
    
    //calculate variance
    //calculate sum along columns
    //printf("\n Soma: \n");
    for(int i=0; i<widthInCU-1; i++){
        soma[i] = 0;
        for(int j=0; j<heightInCU; j++){
            soma[i] = soma[i] + abs(colDiffMap[j][i]);
        }
        //printf("%d\n", soma[i]);
    }
    
    //calculate variance: each element minus the mean, powered to square
    //printf("Variance (pre-sorted): \n");
    for(int i=0; i<widthInCU-1; i++){
        var[i] = 0;
        for(int j=0; j<heightInCU; j++){
            var[i] = var[i] + pow((abs(colDiffMap[j][i])-((Double)soma[i]/(Double)heightInCU)),2);           
        }
        var[i] = var[i] / (heightInCU-1);
        //printf("[%d] - %f\n", i+1, var[i]);
    }    
    
    //store the original variance values before sorting
    memcpy(originalVariance, var, sizeof(var));
    
    //sort variance array, which becomes a variance ranking
    qsort(&var[0],sizeof(var)/sizeof(Double),sizeof(Double),sortDescendingDoubles);
        
    /*
    //print variance array after being sorted    
    printf("\nVariance (sorted): \n");
    for(int i=0; i<widthInCU-1; i++){
        printf("%f\n", var[i]);
    }
    */
    
    //pick the "tilesPerCol" first tile boundary indexes
    //while the number of boundaries picked is different from the number of Tiles per column
    while(boundariesPicked != tilesPerCol){
        if(pickPreviousPattern){
            printf("SUITABLE TILES NOT FOUND: PICK PREVIOUS PATTERN!\n");
            tileBoundaryIdx[0] = 0;                                             
            return tileBoundaryIdx;
        }        
        for(int i=0; i<sizeof(var)/sizeof(Double); i++){                          //iterate on the variance ranking array
            if(boundariesPicked == tilesPerCol)
                break;
            for(int j=0; j<sizeof(originalVariance)/sizeof(Double); j++){         //iterate on the original variance array to pick the indexes
                if(var[i] == originalVariance[j] && boundariesPicked == 0){     //if the best ranked variance is equal to the original, and it is the first boundary to be defined                   
                    if(j+1 >= 4 && j+1 <= widthInCU-4){                             //consist regarding the picture limits
                        tileBoundaryIdx[boundariesPicked] = j+1;                  //pick the first index
                        tileTotalSize = tileBoundaryIdx[boundariesPicked];
                        boundariesPicked++;                                     //increment the number of boundaries picked
                        break;
                    }
                    else
                        break;
                }
                else
                {
                    tileMinSize = true;
                    if(var[i] == originalVariance[j] && boundariesPicked != 0){ //if it is not the first boundary to be defined
                        for(int k=0; k<boundariesPicked; k++){
                            if(abs((j+1)-(Int)tileBoundaryIdx[k]) < 4)                   //consist the Tile size between the Tiles already defined
                                tileMinSize = false;                            //flag that indicates that the min. size is not respected
                        }

                        if(((tileTotalSize+j+1) > widthInCU) || ((tileTotalSize+j+1) == widthInCU && (boundariesPicked+1) != tilesPerCol))
                           tileMinSize = false;
                        
                        if(j+1 >= 4 && j+1 <= widthInCU-4 && tileMinSize){          //consists tile size regarding picture limits and previous picked Tiles
                            tileBoundaryIdx[boundariesPicked] = j+1;              //store the boundary index
                            qsort(&tileBoundaryIdx[0],boundariesPicked+1,sizeof(Int),sortAscending);
                            for(int k=0; k<=boundariesPicked; k++){
                                if(tileBoundaryIdx[k] > tileTotalSize)          //update the Tiles' total size if necessary
                                    tileTotalSize = tileBoundaryIdx[k];
                            }                                  
                            boundariesPicked++;                                 //increment the boundaries already picked
                            break;
                        }
                        else
                            break;
                    }
                }
            }
            if(i == sizeof(var)/sizeof(Double) && boundariesPicked != tilesPerCol){ //if the variance ranking is not suitable to define
                pickPreviousPattern = true;                                         //a Tile pattern, make the flag true to pick the last valid
            }
        }
    }
        
    //UInt* finalTileArray = new UInt[tilesPerCol];                               //array to store Tiles' sizes
    std::vector<Int>finalTileArray;
    finalTileArray[0] = tileBoundaryIdx[0];
    
    for(int i=1; i<boundariesPicked; i++){        
        finalTileArray[i] = tileBoundaryIdx[i]-tileBoundaryIdx[i-1];            //calculus of each size
    }
           
    return finalTileArray;
}
//DI END

Void TComPicSym::create  ( ChromaFormat chromaFormatIDC, Int iPicWidth, Int iPicHeight, UInt uiMaxWidth, UInt uiMaxHeight, UInt uiMaxDepth )
{
  UInt i;

  m_uhTotalDepth      = uiMaxDepth;
  m_uiNumPartitions   = 1<<(m_uhTotalDepth<<1);

  m_uiMaxCUWidth      = uiMaxWidth;
  m_uiMaxCUHeight     = uiMaxHeight;

  m_uiMinCUWidth      = uiMaxWidth  >> m_uhTotalDepth;
  m_uiMinCUHeight     = uiMaxHeight >> m_uhTotalDepth;

  m_uiNumPartInWidth  = m_uiMaxCUWidth  / m_uiMinCUWidth;
  m_uiNumPartInHeight = m_uiMaxCUHeight / m_uiMinCUHeight;

  m_uiWidthInCU       = ( iPicWidth %m_uiMaxCUWidth  ) ? iPicWidth /m_uiMaxCUWidth  + 1 : iPicWidth /m_uiMaxCUWidth;
  m_uiHeightInCU      = ( iPicHeight%m_uiMaxCUHeight ) ? iPicHeight/m_uiMaxCUHeight + 1 : iPicHeight/m_uiMaxCUHeight;

  m_uiNumCUsInFrame   = m_uiWidthInCU * m_uiHeightInCU;
  m_apcTComDataCU     = new TComDataCU*[m_uiNumCUsInFrame];

  if (m_uiNumAllocatedSlice>0)
  {
    for ( i=0; i<m_uiNumAllocatedSlice ; i++ )
    {
      delete m_apcTComSlice[i];
    }
    delete [] m_apcTComSlice;
  }
  m_apcTComSlice      = new TComSlice*[m_uiNumCUsInFrame];
  m_apcTComSlice[0]   = new TComSlice;
  m_uiNumAllocatedSlice = 1;
  for ( i=0; i<m_uiNumCUsInFrame ; i++ )
  {
    m_apcTComDataCU[i] = new TComDataCU;
    m_apcTComDataCU[i]->create( chromaFormatIDC, m_uiNumPartitions, m_uiMaxCUWidth, m_uiMaxCUHeight, false, m_uiMaxCUWidth >> m_uhTotalDepth
#if ADAPTIVE_QP_SELECTION
      , true
#endif
      );
  }

  m_puiCUOrderMap = new UInt[m_uiNumCUsInFrame+1];
  m_puiTileIdxMap = new UInt[m_uiNumCUsInFrame];
  m_puiInverseCUOrderMap = new UInt[m_uiNumCUsInFrame+1];

  for( i=0; i<m_uiNumCUsInFrame; i++ )
  {
    m_puiCUOrderMap[i] = i;
    m_puiInverseCUOrderMap[i] = i;
  }

  m_saoBlkParams = new SAOBlkParam[m_uiNumCUsInFrame];
}

Void TComPicSym::destroy()
{
  if (m_uiNumAllocatedSlice>0)
  {
    for (Int i = 0; i<m_uiNumAllocatedSlice ; i++ )
    {
      delete m_apcTComSlice[i];
    }
    delete [] m_apcTComSlice;
  }
  m_apcTComSlice = NULL;

  for (Int i = 0; i < m_uiNumCUsInFrame; i++)
  {
    m_apcTComDataCU[i]->destroy();
    delete m_apcTComDataCU[i];
    m_apcTComDataCU[i] = NULL;
  }
  delete [] m_apcTComDataCU;
  m_apcTComDataCU = NULL;

  delete [] m_puiCUOrderMap;
  m_puiCUOrderMap = NULL;

  delete [] m_puiTileIdxMap;
  m_puiTileIdxMap = NULL;

  delete [] m_puiInverseCUOrderMap;
  m_puiInverseCUOrderMap = NULL;

  if(m_saoBlkParams)
  {
    delete[] m_saoBlkParams; m_saoBlkParams = NULL;
  }
}

Void TComPicSym::allocateNewSlice()
{
  assert ((m_uiNumAllocatedSlice + 1) <= m_uiNumCUsInFrame);
  m_apcTComSlice[m_uiNumAllocatedSlice ++] = new TComSlice;
  if (m_uiNumAllocatedSlice>=2)
  {
    m_apcTComSlice[m_uiNumAllocatedSlice-1]->copySliceInfo( m_apcTComSlice[m_uiNumAllocatedSlice-2] );
    m_apcTComSlice[m_uiNumAllocatedSlice-1]->initSlice();
  }
}

Void TComPicSym::clearSliceBuffer()
{
  UInt i;
  for (i = 1; i < m_uiNumAllocatedSlice; i++)
  {
    delete m_apcTComSlice[i];
  }
  m_uiNumAllocatedSlice = 1;
}

UInt TComPicSym::getPicSCUEncOrder( UInt SCUAddr )
{
  return getInverseCUOrderMap(SCUAddr/m_uiNumPartitions)*m_uiNumPartitions + SCUAddr%m_uiNumPartitions;
}

UInt TComPicSym::getPicSCUAddr( UInt SCUEncOrder )
{
  return getCUOrderMap(SCUEncOrder/m_uiNumPartitions)*m_uiNumPartitions + SCUEncOrder%m_uiNumPartitions;
}

//DI BEGIN
//adicionando parâmetro ao initTiles
Void TComPicSym::initTiles(floatingClass *acessGOP, TComPPS *pps, TComPic *pcPic)
{

  //teste impressão
  //printf("Imprimindo no PicSym: %d\n", acessGOP->getTestGOP());
  //DI END
  
  //set NumColumnsMinus1 and NumRowsMinus1
  setNumColumnsMinus1( pps->getNumTileColumnsMinus1() );
  setNumRowsMinus1( pps->getTileNumRowsMinus1() );

  const Int numCols = pps->getNumTileColumnsMinus1() + 1;
  const Int numRows = pps->getTileNumRowsMinus1() + 1;
  const Int numTiles = numRows * numCols;
  
  // allocate memory for tile parameters
  m_tileParameters.resize(numTiles);
  
  //DI BEGIN
  //adicionando mais testes
  //printf("Imprimindo no PicSym_GOPSize: %d\n", acessGOP->getGOPSize());
  //DI END
  
  //DI BEGIN
  //pick frame size in CUs - Cauane
    Int heightInCU = acessGOP->frameHeightInCU;
    Int widthInCU = acessGOP->frameWidthInCU;        
        
   if(firstFrame){        
       //initialize the previous Tile partitioning pattern - Cauane
       //previousTilesRowSizes = new UInt[numRows - 1];
       //previousTilesColSizes = new UInt[numCols - 1];
        
       //matrix initialization - Cauane
       bitMtx = new UInt*[heightInCU];
       varianceMap = new UInt*[heightInCU];
       LCUPartMtx = new UInt*[heightInCU];
       for(int k=0; k<heightInCU; ++k){
           bitMtx[k] = new UInt[widthInCU];        
           varianceMap[k] = new UInt[widthInCU];
           LCUPartMtx[k] = new UInt[widthInCU];
       }
   }
   //DI END
    
  if( pps->getTileUniformSpacingFlag() )
  {
    //DI BEGIN;
    firstFrame = 0; //after creating the bitMtx, the flag is set to 0 - Cauane
    //DI END
      //set width and height for each (uniform) tile
    for(Int row=0; row < numRows; row++)
    {
      for(Int col=0; col < numCols; col++)
      {
        const Int tileIdx = row * numCols + col;
        m_tileParameters[tileIdx].setTileWidth( (col+1)*getFrameWidthInCU()/numCols
                                              - (col*getFrameWidthInCU())/numCols );
        m_tileParameters[tileIdx].setTileHeight( (row+1)*getFrameHeightInCU()/numRows
                                               - (row*getFrameHeightInCU())/numRows );
      }
    }
  }
  else
  {
    //if the adaptive partitioning technique is enabled, employ the algorithm - Cauane
        //if it is not, use the Tile's sizes defined in the cfg. file
        if(acessGOP->adaptiveTilesFlag == 1){
            UInt rowBoundaries = getNumRowsMinus1();
            UInt colBoundaries = getNumColumnsMinus1();
            //UInt* tilesRowSizes = new UInt[rowBoundaries+1]; //Array to store the size of each Tile row
            //modificando tipo variaveis
            std::vector<Int>tilesRowSizes;
            //UInt* auxTilesColumnSizes = new UInt[colBoundaries+1]; //Array to store the size of each Tile column
            std::vector<Int>tilesColumnSizes;
            
            //if it is the first frame and the first frame tiling method is uniform or none (default), adopt uniform spaced division - Cauane
            if(firstFrame && (acessGOP->firstFrameTilingMethod == "" || acessGOP->firstFrameTilingMethod == "u" || acessGOP->firstFrameTilingMethod == "U" || acessGOP->firstFrameTilingMethod == "uniform" || acessGOP->firstFrameTilingMethod == "Uniform" || acessGOP->firstFrameTilingMethod == "UNIFORM")){                                                                                            
                //Set uniform spaced tile height
                for(int p=0; p < getNumRowsMinus1()+1; p++){               
                    tilesRowSizes[p] = ((p+1)*getFrameHeightInCU()/(getNumRowsMinus1()+1)) - ((p*getFrameHeightInCU())/(getNumRowsMinus1()+1));                      
                    printf("tRowSize[%d] = %d\n", p, tilesRowSizes[p]);
                }

                //Set uniform spaced tile width
                for(int p=0; p < getNumColumnsMinus1()+1; p++){               
                    tilesColumnSizes[p] = ((p+1)*getFrameWidthInCU()/(getNumColumnsMinus1()+1)) - ((p*getFrameWidthInCU())/(getNumColumnsMinus1()+1));
                    printf("tColSize[%d] = %d\n", p, tilesColumnSizes[p]);                
                }

                //Store the values from the input file into the HM variables
                pps->setTileColumnWidth(tilesColumnSizes);
                pps->setTileRowHeight(tilesRowSizes);

                
                //free memory space
                tilesRowSizes.clear();
                tilesColumnSizes.clear();   
                               
                firstFrame = 0; //set the flag to 0, after defining the uniform spaced Tiles for the first frame - Cauane
            }
            else{
                if(firstFrame && (acessGOP->firstFrameTilingMethod == "v" || acessGOP->firstFrameTilingMethod == "V" || acessGOP->firstFrameTilingMethod == "variance" || acessGOP->firstFrameTilingMethod == "Variance" || acessGOP->firstFrameTilingMethod == "VARIANCE")){
                    varianceMap = variancePerLCU(pcPic);
                    
                    tilesRowSizes = getRowTileBoundary(varianceMap, rowBoundaries, heightInCU, widthInCU);
                    tilesColumnSizes = getColTileBoundary(varianceMap, colBoundaries, heightInCU, widthInCU);
                    
                    //print Tiles' size
                    for(int p=0; p < getNumRowsMinus1(); p++){                            
                        printf("tRowSize[%d] = %d\n", p, tilesRowSizes[p]);
                    }

                    for(int p=0; p < getNumColumnsMinus1(); p++){                               
                        printf("tColSize[%d] = %d\n", p, tilesColumnSizes[p]);
                    }
                    
                    //Store the values from the input file into the HM variables
                    pps->setTileColumnWidth(tilesColumnSizes);
                    pps->setTileRowHeight(tilesRowSizes);

                    //free memory space
                    //delete[] tilesRowSizes;
                    tilesRowSizes.clear();
                    //delete[] tilesColumnSizes;
                    tilesColumnSizes.clear();
                    
                    firstFrame = 0; //set the flag to 0, after defining the uniform spaced Tiles for the first frame - Cauane
                }                                               
                else
                {                                                          
                    if(acessGOP->tilingMethod == "varmap" || acessGOP->tilingMethod == "v" || acessGOP->tilingMethod == "Varmap" || acessGOP->tilingMethod == "V" || acessGOP->tilingMethod == "VARMAP" || acessGOP->tilingMethod == ""){
                        //extract variance map
                        varianceMap = variancePerLCU(pcPic);
                        
                        //determine partitioning based on the frame variance map
                        tilesRowSizes = getRowTileBoundary(varianceMap, rowBoundaries, heightInCU, widthInCU);
                        tilesColumnSizes = getColTileBoundary(varianceMap, colBoundaries, heightInCU, widthInCU);
                    }
                    if(acessGOP->tilingMethod == "lcupart" || acessGOP->tilingMethod == "l" || acessGOP->tilingMethod == "LCUPart" || acessGOP->tilingMethod == "L" || acessGOP->tilingMethod == "LCUPART" || acessGOP->tilingMethod == "LCU"){
                        //extract LCU partitions matrix
                        LCUPartMtx = genPartitionsMtx(acessGOP->sumLCUPartitions, heightInCU, widthInCU);
                                                
                        //determine partitioning based on the frame LCU partitioning map
                        tilesRowSizes = getRowTileBoundary(LCUPartMtx, rowBoundaries, heightInCU, widthInCU);
                        tilesColumnSizes = getColTileBoundary(LCUPartMtx, colBoundaries, heightInCU, widthInCU);
                    }
                    if(acessGOP->tilingMethod == "bitmap" || acessGOP->tilingMethod == "b" || acessGOP->tilingMethod == "Bitmap" || acessGOP->tilingMethod == "B" || acessGOP->tilingMethod == "BITMAP"){
                        if(acessGOP->refreshTiling == 1 && acessGOP->m_totalCoded % acessGOP->m_iGopSize == 0 &&  !firstFrame){
                            //extract variance map
                            varianceMap = variancePerLCU(pcPic);

                            //determine partitioning based on the frame variance map
                            tilesRowSizes = getRowTileBoundary(varianceMap, rowBoundaries, heightInCU, widthInCU);
                            tilesColumnSizes = getColTileBoundary(varianceMap, colBoundaries, heightInCU, widthInCU);
                        }
                        else{                        
                            //determine partitioning based on the frame bit map
                            tilesRowSizes = getRowTileBoundary(bitMtx, rowBoundaries, heightInCU, widthInCU);
                            tilesColumnSizes = getColTileBoundary(bitMtx, colBoundaries, heightInCU, widthInCU);
                        }   
                    }
                    
                    //if a suitable Tile partitioning pattern could not be picked, consider the previous valid one - Cauane
                    if(tilesRowSizes[0] == 0 && rowBoundaries > 0)
                        tilesRowSizes = previousTilesRowSizes;            
                    else
                        previousTilesRowSizes = tilesRowSizes;

                    if(tilesColumnSizes[0] == 0 && colBoundaries > 0)
                        tilesColumnSizes = previousTilesColSizes;            
                    else
                        previousTilesColSizes = tilesColumnSizes;

                    //print Tiles' size
                    for(int p=0; p < getNumRowsMinus1(); p++){                            
                        printf("tRowSize[%d] = %d\n", p, tilesRowSizes[p]);
                    }

                    for(int p=0; p < getNumColumnsMinus1(); p++){                               
                        printf("tColSize[%d] = %d\n", p, tilesColumnSizes[p]);
                    }               

                    //Store the values from the input file into the HM variables
                    pps->setTileColumnWidth(tilesColumnSizes);
                    pps->setTileRowHeight(tilesRowSizes);

                    //free memory space
                    tilesRowSizes.clear();
                    tilesColumnSizes.clear();
                }
            }  
        //DI END
    }
        
    //set the width for each tile
    for(Int row=0; row < numRows; row++)
    {
      Int cumulativeTileWidth = 0;
      for(Int col=0; col < getNumColumnsMinus1(); col++)
      {
        m_tileParameters[row * numCols + col].setTileWidth( pps->getTileColumnWidth(col) );
        cumulativeTileWidth += pps->getTileColumnWidth(col);
      }
      m_tileParameters[row * numCols + getNumColumnsMinus1()].setTileWidth( getFrameWidthInCU()-cumulativeTileWidth );
    }

    //set the height for each tile
    for(Int col=0; col < numCols; col++)
    {
      Int cumulativeTileHeight = 0;
      for(Int row=0; row < getNumRowsMinus1(); row++)
      {
        m_tileParameters[row * numCols + col].setTileHeight( pps->getTileRowHeight(row) );
        cumulativeTileHeight += pps->getTileRowHeight(row);
      }
      m_tileParameters[getNumRowsMinus1() * numCols + col].setTileHeight( getFrameHeightInCU()-cumulativeTileHeight );
    }
 
  }

#if TILE_SIZE_CHECK
  Int minWidth  = 1;
  Int minHeight = 1;
  const Int profileIdc = pps->getSPS()->getPTL()->getGeneralPTL()->getProfileIdc();
  if (  profileIdc == Profile::MAIN || profileIdc == Profile::MAIN10) //TODO: RExt - add more profiles...
  {
    if (pps->getTilesEnabledFlag())
    {
      minHeight = 64  / g_uiMaxCUHeight;
      minWidth  = 256 / g_uiMaxCUWidth;
    }
  }
  for(Int row=0; row < numRows; row++)
  {
    for(Int col=0; col < numCols; col++)
    {
      const Int tileIdx = row * numCols + col;
      assert (m_tileParameters[tileIdx].getTileWidth() >= minWidth);
      assert (m_tileParameters[tileIdx].getTileHeight() >= minHeight);
    }
  }
#endif

  //initialize each tile of the current picture
  for( Int row=0; row < numRows; row++ )
  {
    for( Int col=0; col < numCols; col++ )
    {
      const Int tileIdx = row * numCols + col;

      //initialize the RightEdgePosInCU for each tile
      Int rightEdgePosInCTU = 0;
      for( Int i=0; i <= col; i++ )
      {
        rightEdgePosInCTU += m_tileParameters[row * numCols + i].getTileWidth();
      }
      m_tileParameters[tileIdx].setRightEdgePosInCU(rightEdgePosInCTU-1);

      //initialize the BottomEdgePosInCU for each tile
      Int bottomEdgePosInCTU = 0;
      for( Int i=0; i <= row; i++ )
      {
        bottomEdgePosInCTU += m_tileParameters[i * numCols + col].getTileHeight();
      }
      m_tileParameters[tileIdx].setBottomEdgePosInCU(bottomEdgePosInCTU-1);

      //initialize the FirstCUAddr for each tile
      m_tileParameters[tileIdx].setFirstCUAddr( (m_tileParameters[tileIdx].getBottomEdgePosInCU() - m_tileParameters[tileIdx].getTileHeight() + 1) * getFrameWidthInCU() + 
                                                 m_tileParameters[tileIdx].getRightEdgePosInCU() - m_tileParameters[tileIdx].getTileWidth() + 1);
    }
  }

  Int  columnIdx = 0;
  Int  rowIdx = 0;

  //initialize the TileIdxMap
  for( Int i=0; i<m_uiNumCUsInFrame; i++)
  {
    for( Int col=0; col < numCols; col++)
    {
      if(i % getFrameWidthInCU() <= m_tileParameters[col].getRightEdgePosInCU())
      {
        columnIdx = col;
        break;
      }
    }
    for(Int row=0; row < numRows; row++)
    {
      if(i / getFrameWidthInCU() <= m_tileParameters[row*numCols].getBottomEdgePosInCU())
      {
        rowIdx = row;
        break;
      }
    }
    m_puiTileIdxMap[i] = rowIdx * numCols + columnIdx;
  }
}

UInt TComPicSym::xCalculateNxtCUAddr( UInt uiCurrCUAddr )
{
  UInt  uiNxtCUAddr;
  UInt  uiTileIdx;

  //get the tile index for the current LCU
  uiTileIdx = this->getTileIdxMap(uiCurrCUAddr);

  //get the raster scan address for the next LCU
  if( uiCurrCUAddr % m_uiWidthInCU == this->getTComTile(uiTileIdx)->getRightEdgePosInCU() && uiCurrCUAddr / m_uiWidthInCU == this->getTComTile(uiTileIdx)->getBottomEdgePosInCU() )
  //the current LCU is the last LCU of the tile
  {
    if(uiTileIdx == (m_iNumColumnsMinus1+1)*(m_iNumRowsMinus1+1)-1)
    {
      uiNxtCUAddr = m_uiNumCUsInFrame;
    }
    else
    {
      uiNxtCUAddr = this->getTComTile(uiTileIdx+1)->getFirstCUAddr();
    }
  }
  else //the current LCU is not the last LCU of the tile
  {
    if( uiCurrCUAddr % m_uiWidthInCU == this->getTComTile(uiTileIdx)->getRightEdgePosInCU() )  //the current LCU is on the rightmost edge of the tile
    {
      uiNxtCUAddr = uiCurrCUAddr + m_uiWidthInCU - this->getTComTile(uiTileIdx)->getTileWidth() + 1;
    }
    else
    {
      uiNxtCUAddr = uiCurrCUAddr + 1;
    }
  }

  return uiNxtCUAddr;
}

Void TComPicSym::deriveLoopFilterBoundaryAvailibility(Int ctu,
                                                      Bool& isLeftAvail,
                                                      Bool& isRightAvail,
                                                      Bool& isAboveAvail,
                                                      Bool& isBelowAvail,
                                                      Bool& isAboveLeftAvail,
                                                      Bool& isAboveRightAvail,
                                                      Bool& isBelowLeftAvail,
                                                      Bool& isBelowRightAvail
                                                      )
{

  isLeftAvail      = (ctu % m_uiWidthInCU != 0);
  isRightAvail     = (ctu % m_uiWidthInCU != m_uiWidthInCU-1);
  isAboveAvail     = (ctu >= m_uiWidthInCU );
  isBelowAvail     = (ctu <  m_uiNumCUsInFrame - m_uiWidthInCU);
  isAboveLeftAvail = (isAboveAvail && isLeftAvail);
  isAboveRightAvail= (isAboveAvail && isRightAvail);
  isBelowLeftAvail = (isBelowAvail && isLeftAvail);
  isBelowRightAvail= (isBelowAvail && isRightAvail);

  Bool isLoopFiltAcrossTilePPS = getCU(ctu)->getSlice()->getPPS()->getLoopFilterAcrossTilesEnabledFlag();

  {
    TComDataCU* ctuCurr  = getCU(ctu);
    TComDataCU* ctuLeft  = isLeftAvail ?getCU(ctu-1):NULL;
    TComDataCU* ctuRight = isRightAvail?getCU(ctu+1):NULL;
    TComDataCU* ctuAbove = isAboveAvail?getCU(ctu-m_uiWidthInCU):NULL;
    TComDataCU* ctuBelow = isBelowAvail?getCU(ctu+m_uiWidthInCU):NULL;
    TComDataCU* ctuAboveLeft  = isAboveLeftAvail ? getCU(ctu-m_uiWidthInCU-1):NULL;
    TComDataCU* ctuAboveRigtht= isAboveRightAvail? getCU(ctu-m_uiWidthInCU+1):NULL;
    TComDataCU* ctuBelowLeft  = isBelowLeftAvail ? getCU(ctu+m_uiWidthInCU-1):NULL;
    TComDataCU* ctuBelowRight = isBelowRightAvail? getCU(ctu+m_uiWidthInCU+1):NULL;

    //DI BEGIN
    //std::cout << "getCU: " << getCU(ctu+1) << " - ctu: " << ctu+1 << '\n';
    //fputs(isRightAvail ? "Right: true\n" : "Right: false\n", stdout);
    //fputs(isLeftAvail ? "Left: true\n" : "Left: false\n", stdout);
    //DI END
    {
      //left
      if(ctuLeft != NULL)
      {
        //printf("ctuLeft: %d\n", ctuLeft->getSlice()->getSliceCurStartCUAddr());
        isLeftAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuLeft->getSlice()->getSliceCurStartCUAddr())?ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //above
      if(ctuAbove != NULL)
      {
        isAboveAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuAbove->getSlice()->getSliceCurStartCUAddr())?ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //right
      if(ctuRight != NULL)
      {
          //DI BEGIN
          //printf("ctuRight 3: %d\n", ctuRight->getAddr());
          //printf("PRINT 1 - %d\n", ctuCurr->getSlice()->getSliceCurStartCUAddr());
          //printf("AQUI 3\n");
          //printf("PRINT 2 - %d\n", ctuRight->getSlice()->getSliceCurStartCUAddr());
          //printf("PRINT 3 - %d\n", ctuRight->getSlice()->getSliceCurStartCUAddr());
          //printf("AQUI 4\n");
          //DI END
        isRightAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuRight->getSlice()->getSliceCurStartCUAddr())?ctuRight->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //below
      if(ctuBelow != NULL)
      {
        isBelowAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuBelow->getSlice()->getSliceCurStartCUAddr())?ctuBelow->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //above-left
      if(ctuAboveLeft != NULL)
      {
        isAboveLeftAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuAboveLeft->getSlice()->getSliceCurStartCUAddr())?ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //below-right
      if(ctuBelowRight != NULL)
      {
        isBelowRightAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuBelowRight->getSlice()->getSliceCurStartCUAddr())?ctuBelowRight->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }


      //above-right
      if(ctuAboveRigtht != NULL)
      {
        Int curSliceStartEncOrder  = ctuCurr->getSlice()->getSliceCurStartCUAddr();
        Int aboveRigthtSliceStartEncOrder = ctuAboveRigtht->getSlice()->getSliceCurStartCUAddr();

        isAboveRightAvail = (curSliceStartEncOrder == aboveRigthtSliceStartEncOrder)?(true):
          (
          (curSliceStartEncOrder > aboveRigthtSliceStartEncOrder)?(ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag())
          :(ctuAboveRigtht->getSlice()->getLFCrossSliceBoundaryFlag())
          );          
      }
      //below-left
      if(ctuBelowLeft != NULL)
      {
        Int curSliceStartEncOrder  = ctuCurr->getSlice()->getSliceCurStartCUAddr();
        Int belowLeftSliceStartEncOrder = ctuBelowLeft->getSlice()->getSliceCurStartCUAddr();

        isBelowLeftAvail = (curSliceStartEncOrder == belowLeftSliceStartEncOrder)?(true):
          (
          (curSliceStartEncOrder > belowLeftSliceStartEncOrder)?(ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag())
          :(ctuBelowLeft->getSlice()->getLFCrossSliceBoundaryFlag())
          );
      }        
    }

    if(!isLoopFiltAcrossTilePPS)
    {
      isLeftAvail      = (!isLeftAvail      ) ?false:(getTileIdxMap( ctuLeft->getAddr()         ) == getTileIdxMap( ctu ));
      isAboveAvail     = (!isAboveAvail     ) ?false:(getTileIdxMap( ctuAbove->getAddr()        ) == getTileIdxMap( ctu ));
      isRightAvail     = (!isRightAvail     ) ?false:(getTileIdxMap( ctuRight->getAddr()        ) == getTileIdxMap( ctu ));
      isBelowAvail     = (!isBelowAvail     ) ?false:(getTileIdxMap( ctuBelow->getAddr()        ) == getTileIdxMap( ctu ));
      isAboveLeftAvail = (!isAboveLeftAvail ) ?false:(getTileIdxMap( ctuAboveLeft->getAddr()    ) == getTileIdxMap( ctu ));
      isAboveRightAvail= (!isAboveRightAvail) ?false:(getTileIdxMap( ctuAboveRigtht->getAddr()  ) == getTileIdxMap( ctu ));
      isBelowLeftAvail = (!isBelowLeftAvail ) ?false:(getTileIdxMap( ctuBelowLeft->getAddr()    ) == getTileIdxMap( ctu ));
      isBelowRightAvail= (!isBelowRightAvail) ?false:(getTileIdxMap( ctuBelowRight->getAddr()   ) == getTileIdxMap( ctu ));
    }
  }

}


TComTile::TComTile()
: m_uiTileWidth         (0)
, m_uiTileHeight        (0)
, m_uiRightEdgePosInCU  (0)
, m_uiBottomEdgePosInCU (0)
, m_uiFirstCUAddr       (0)
{
}

TComTile::~TComTile()
{
}
//! \}
