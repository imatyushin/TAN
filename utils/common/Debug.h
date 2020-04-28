#pragma once

#ifndef CLQUEUE_REFCOUNT
#define CLQUEUE_REFCOUNT( clqueue ) { \
		cl_uint refcount = 0; \
		clGetCommandQueueInfo(clqueue, CL_QUEUE_REFERENCE_COUNT, sizeof(refcount), &refcount, NULL); \
		printf("\nFILE:%s line:%d Queue %llX ref count: %d\r\n", __FILE__ , __LINE__, clqueue, refcount); \
}
#endif

#ifndef DBG_CLRELEASE
#define DBG_CLRELEASE( clqueue, qname ) { \
		cl_uint refcount = 0; \
		clReleaseCommandQueue(clqueue); \
		clGetCommandQueueInfo(clqueue, CL_QUEUE_REFERENCE_COUNT, sizeof(refcount), &refcount, NULL); \
		printf("\nFILE:%s line:%d %s %llX ref count: %d\r\n", __FILE__ , __LINE__,qname, clqueue, refcount); \
}
#endif