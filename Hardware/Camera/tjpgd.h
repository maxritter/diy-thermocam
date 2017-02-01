/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor include file               (C)ChaN, 2012
/----------------------------------------------------------------------------*/
#ifndef _TJPGDEC
#define _TJPGDEC
/*---------------------------------------------------------------------------*/
/* System Configurations */

#define	JD_SZBUF		512	/* Size of stream input buffer */
#define JD_FORMAT		1	/* Output pixel format 0:RGB888 (3 unsigned char/pix), 1:RGB565 (1 unsigned short/pix) */
#define	JD_USE_SCALE	0	/* Use descaling feature for output */
#define JD_TBLCLIP		1	/* Use table for saturation (might be a bit faster but increases 1K bytes of code size) */

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif


/* Error code */
typedef enum {
	JDR_OK = 0,	/* 0: Succeeded */
	JDR_INTR,	/* 1: Interrupted by output function */	
	JDR_INP,	/* 2: Device error or wrong termination of input stream */
	JDR_MEM1,	/* 3: Insufficient memory pool for the image */
	JDR_MEM2,	/* 4: Insufficient stream input buffer */
	JDR_PAR,	/* 5: Parameter error */
	JDR_FMT1,	/* 6: Data format error (may be damaged data) */
	JDR_FMT2,	/* 7: Right format but not supported */
	JDR_FMT3	/* 8: Not supported JPEG standard */
} JRESULT;



/* Rectangular structure */
typedef struct {
	unsigned short left, right, top, bottom;
} JRECT;



/* Decompressor object structure */
typedef struct JDEC JDEC;
struct JDEC {
	unsigned int dctr;				/* Number of bytes available in the input buffer */
	unsigned char* dptr;				/* Current data read ptr */
	unsigned char* inbuf;			/* Bit stream input buffer */
	unsigned char dmsk;				/* Current bit in the current read byte */
	unsigned char scale;				/* Output scaling ratio */
	unsigned char msx, msy;			/* MCU size in unit of block (width, height) */
	unsigned char qtid[3];			/* Quantization table ID of each component */
	short dcv[3];			/* Previous DC element of each component */
	unsigned short nrst;				/* Restart inverval */
	unsigned int width, height;		/* Size of the input image (pixel) */
	unsigned char* huffbits[2][2];	/* Huffman bit distribution tables [id][dcac] */
	unsigned short* huffcode[2][2];	/* Huffman code word tables [id][dcac] */
	unsigned char* huffdata[2][2];	/* Huffman decoded data tables [id][dcac] */
	long* qttbl[4];			/* Dequaitizer tables [id] */
	void* workbuf;			/* Working buffer for IDCT and RGB output */
	unsigned char* mcubuf;			/* Working buffer for the MCU */
	void* pool;				/* Pointer to available memory pool */
	unsigned int sz_pool;			/* Size of momory pool (bytes available) */
	unsigned int (*infunc)(JDEC*, unsigned char*, unsigned int);/* Pointer to jpeg stream input function */
	void* device;			/* Pointer to I/O device identifiler for the session */
};



/* TJpgDec API functions */
JRESULT jd_prepare (JDEC*, unsigned int(*)(JDEC*,unsigned char*,unsigned int), void*, unsigned int, void*);
JRESULT jd_decomp (JDEC*, unsigned int(*)(JDEC*,void*,JRECT*), unsigned char);


#ifdef __cplusplus
}
#endif

#endif /* _TJPGDEC */
