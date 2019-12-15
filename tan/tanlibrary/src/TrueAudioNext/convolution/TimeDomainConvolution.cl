// Max LDS size: 64KB = 65536 Byte = 16384 float element
// Max convolution length for LDS float to work: 16384 - Lx (local size)

__kernel
//__kernel __attribute__((reqd_work_group_size(LocalSize, 1, 1)))
void TimeDomainConvolution(
__global    float*  histBuf,	   ///< [in]	
int         convLength,            ///< [in] convolution length
int         bufPos,                ///< [in]
int         dataLength,            ///< [in] Size of the buffer processed per kernel run
int         firstNonZero,          ///< [in]
int         lastNonZero,           ///< [in]
__global    float*  filter,        ///< [in]
__global    float*  pOutput        ///< [out]
)
{
    int grpid = get_group_id(0);
    int thrdid = get_local_id(0);
    int grptot = get_num_groups(0);
    int grpsz = get_local_size(0);

    int srcOffset = grpid*grpsz;
    int endOffset = srcOffset + grpsz;

    if (endOffset > dataLength)
        endOffset = dataLength;

    //if (get_global_id(0) > inputSize)
     //   return;

    //int ldsSize = FilterLength + LocalSize;
    //__local float localBuffer[FilterLength + LocalSize];

    //int readPerWorkItem = ldsSize / LocalSize;

    //for (int j = 0; j < datalength; j++){
    for (int j = srcOffset; j < endOffset; j++){
            pOutput[j] = 0.0;
        for (int k = firstNonZero; k < lastNonZero; k++){
            pOutput[j] += histBuf[(bufPos + j - k + convLength) % convLength] * filter[k];
        }
    }
}