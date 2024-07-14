#
/*
 *    Copyright (C) 2015
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of DAB library
 *    DAB library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    DAB library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DAB library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#
#include	"dab-constants.h"
#include	"data-processor.h"
#include	"virtual-datahandler.h"
#include	"mot-handler.h"
#include        "tdc-datahandler.h"

// NOTES RS DATA TABLE TRANSFER VERIFIED!


//      11-bit from HandleFIG0Extension13, see ETSI TS 101 756 table 16
// AppType -> https://www.etsi.org/deliver/etsi_ts/101700_101799/101756/02.02.01_60/ts_101756v020201p.pdf
char *getUserApplicationType (int16_t appType) {
	char *buffer = (char *)malloc(30);
        switch (appType) {
           case 1:     return "Dynamic labels (X-PAD only)";
           case 2:     return "MOT Slide Show";		// ETSI TS 101 499
           case 3:     return "MOT Broadcast Web Site";
           case 4:     return "TPEG";			// ETSI TS 103 551
           case 5:     return "DGPS";
           case 6:     return "TMC";
           case 7:     return "SPI, was EPG";		// ETSI TS 102 818
           case 8:     return "DAB Java";
           case 9:     return "DMB";			// ETSI TS 102 428
           case 0x00a: return "IPDC services";
           case 0x00b: return "Voice applications";
           case 0x00c: return "Middleware";
           case 0x00d: return "Filecasting";		// ETSI TS 102 979
           case 0x44a: return "Journaline";
           default:
	       sprintf(buffer, "(0x%04x)", appType);;
	       return buffer;
        }
        return "";
}

const char *getPacketType (int8_t PacketType) {
        switch (PacketType) {
           case 0:     return "I"; // Intermediate
           case 1:     return "L"; // Last
           case 2:     return "F"; // First
        }
        return "S"; // Single
}

/* void int8ToBitArr(int8_t val, int8_t* arr, int startIndex) {
    for (int i = 7; i >= 0; i--) {
        arr[startIndex + i] = (val >> (7 - i)) & 0x01;
    }
}*/

/* const char *print_buffer_hex(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        std::printf("%02X ", buffer[i]);
    }
    std::printf("\n");
    return " ";
}*/


//	\class dataProcessor
//	The main function of this class is to assemble the 
//	MSCdatagroups and dispatch to the appropriate handler
//
//	fragmentsize == Length * CUSize
	dataProcessor::dataProcessor	(int16_t	bitRate,
	                                 packetdata	*pd,
	                                 API_struct	*p,
	                                 void	        *ctx):
	                                 my_rsDecoder (8, 0435, 0, 1, 10) { // 0..9 -> 10 / 0..15 -> 16
/* NOTE mp4Processor:     my_rsDecoder (8, 0435, 0, 1, 10),
	
	Reed-Solomon RS(120, 110, t = 5) shortened code (see note 1), derived from the original
	systematic RS(255, 245, t = 5) code, shall be applied to 110 byte portions of each audio
	super frame to generate an error protected packet .
	The Reed-Solomon code has length 120 bytes, dimension 110 bytes and allows the correction
	of up to 5 random erroneous bytes in a received word of 120 bytes. 
	
	The shortened Reed-Solomon code may be implemented by adding 135 bytes, all set to zero,
	before the information bytes at the input of an RS(255, 245, t = 5) encoder. After the RS
	coding procedure these null bytes shall be discarded, leading to a RS code word of N = 120
	bytes. 
		
	reedSolomon (uint16_t symsize   = 8,	// symbol size, bits (1-8)
		     uint16_t gfpoly    = 0435, // Field generator polynomial coefficients
                     uint16_t fcr       = 0,	// first root of RS code generator polynomial, index form, 0
                     uint16_t prim      = 1,	// primitive element to generate polynomial roots
                     uint16_t nroots    = 10);	// RS code generator polynomial degree (number of roots)
	                     
	to be adjusted -> my_rsDecoder (				

	The code used is the Reed-Solomon RS (204,188, t = 8) shortened code (see note 2), derived
	from the original systematic RS (255,239, t = 8) code.
	The Reed-Solomon codeword has length 204 bytes, dimension 188 bytes and allows up to 8 random
	erroneous bytes in a received word of 204 bytes to be corrected. 
	
	The shortened Reed-Solomon code may be implemented by adding 51 bytes, all set to zero,
	before the information bytes (i.e. one row of the Application Data Table) at the input of an
	RS (255,239, t = 8) encoder. After the RS coding procedure these null bytes shall be discarded,
	leading to a RS codeword of N = 204 byte
	
	NOTE The primitive polynomials used for the Galois Fields are specified in octal below:
	     Octal 435 -> w= 8 
	                  w= 9: 01021
	                  w=10: 02011
	Ref. http://web.eecs.utk.edu/~jplank/plank/papers/CS-05-570.html
	*/
	this	-> bitRate		= pd -> bitRate;
	this	-> DSCTy		= pd -> DSCTy;
	this	-> appType		= pd -> appType;
	this	-> packetAddress	= pd -> packetAddress;
	this	-> DGflag		= pd -> DGflag;
	this	-> FEC_scheme		= pd -> FEC_scheme;
	this	-> bytesOut		= p  -> bytesOut_Handler;
	this    -> mscQuality   	= p -> program_quality_Handler; // added S. Juhl
	this	-> ctx			= ctx;
        RSDims                  	= 12; 	// mp4: 8 -> 12 rows
        //frameBytes. resize (RSDims * 204); 	// (RSDims * 120); // 2.448 Bytes (2.256 Bytes of Application Data Table + 192 Bytes RS Data Table)
        frameBytes.resize(0);
        outVector.  resize (RSDims * 188); 	// (RSDims * 110); // 2.256 Bytes Application Data Table
        blockFillIndex  = 0;
        blocksInBuffer  = 0;
        
        frameCount      = 0;
        frameErrors     = 0;
        rsErrors        = 0;
        crcErrors	= 0;

        frame_quality   = 0;
        rs_quality      = 0;
//        for (uint8_t i=0; i < 2448; i ++) frameBytes[i]=0;


	
//	const char *appTypeName = getUserApplicationType(pd -> appType);
//	printf("Application Type Name: %s\n", appTypeName);
	
//	fprintf (stderr, "DP: DSCTy=%d app=%s dataHandler=", pd -> DSCTy, appTypeName);
//	fprintf (stderr, "DP: DSCTy=%d app=%d\n", pd -> DSCTy, getUserApplicationType(pd -> appType));
	fprintf (stderr, "** dataProcessor: appType=%d FEC=%d DSCTy=%d (", pd -> appType, FEC_scheme, pd -> DSCTy);
	switch (DSCTy) {
	   default:
	      fprintf(stderr,"virtual)\n");
	      my_dataHandler	= new virtual_dataHandler ();
	      break;

	   case 5:			// do know yet
	      fprintf(stderr,"TDC)\n");
	      my_dataHandler	= new tdc_dataHandler (appType, bytesOut, ctx);
	      break;

// /opt/qt-dab/src/backend/data/data-processor.cpp
/*           case 44:
              my_dataHandler    = new journaline_dataHandler();
              break;

           case 59:
              my_dataHandler    = new ip_dataHandler (mr, dataBuffer);
              break;
*/

	   case 60:
	      fprintf(stderr,"MOT)\n");
	      my_dataHandler	= new motHandler (p -> motdata_Handler, ctx);
	      break;
	}
	packetState	= 0;
}

	dataProcessor::~dataProcessor	(void) {
	delete		my_dataHandler;
}


void	dataProcessor::addtoFrame (uint8_t *outV) {
//	There is - obviously - some exception, that is
//	when the DG flag is on and there are no datagroups for DSCTy5
	/* MSC Quality report */
        if (++frameCount >= 100) {
              frameCount = 0;
              frame_quality     = 1 * (100 - frameErrors);
              rs_quality = 1 * (100 - rsErrors);
              if (mscQuality != nullptr)
                 mscQuality (frame_quality, rs_quality, 1*(100-crcErrors), ctx);
              frameErrors = crcErrors = rsErrors = 0;
        }

	if ((this -> DSCTy == 5) &&
	      (this -> DGflag)) {			// no datagroups
	      handleTDCAsyncstream (outV, 24 * bitRate);
	} else {
	      handlePackets (outV, 24 * bitRate);	// Data group packets
	}
}
//
//	While for a full mix data and audio there will be a single packet in a
//	data compartment, for an empty mix, there may be many more
//	TPEG		appType=4	FEC=1 344
//	TPEG_MM		appType=4	FEC=0 152
//	PPP-RTK-AdV	appType=1500 	FEC=1 192
void	dataProcessor::handlePackets (uint8_t *data, int16_t length) {
int16_t	packetLength	= (getBits_2 (data, 0) + 1) * 24;	// 24-96 bytes
int16_t	continuityIndex	= getBits_2 (data, 2);			// linking packages
int16_t	firstLast	= getBits_2 (data, 4);			// packet type
int16_t	address		= getBits   (data, 6, 10);		// 0 -> only for padding
uint16_t command	= getBits_1 (data, 16);			// 0 data / 1 command packet
int16_t	usefulLength	= getBits_7 (data, 17);			// unsigned binary number of bytes ..91
uint8_t Counter 	= getBits_4 (data, 2);
uint16_t i,j;
	(void)continuityIndex;
	(void)command;

	if ( FEC_scheme == 1 ) {
	      if (address == 1022) {
		    fprintf(stderr,"** DBG: FEC: Adding RS Data Field packet #%d [%d] adr=%d**\n",continuityIndex*4+firstLast,packetLength,address);
	            handleRSDF(data);
	            return;
	      } else if ( address == 1022 && packetState == 2 ) { // in case last RS data field packet gets lost
                    fprintf(stderr,"** DBG FLOW -FEC- PS=%d PL=%d CI=%d FL=%d adr=%d cmd=%d UL=%d\n",packetState,packetLength,continuityIndex,firstLast,address,command,usefulLength);
	            applyFEC();
	            return;
	      } // continue adding normal packets
	} 
	
	if (address != packetAddress || address == 0) {	// sorry, other stream or padding -> discard
	      if (usefulLength > 0) {
	            fprintf(stderr,"** DBG Address mismatch %d != %d -> [%d] 0x",address,packetAddress,usefulLength);
	      	    for (i = 0; i < usefulLength; i++) fprintf(stderr,"%02X",getBits(data,24+i*8,i));
	      	    fprintf(stderr,"\n");
	      } 
	      frameErrors ++;
	      return;
	}
	
	// According to Annex E / ITU-T X.25 ... session header ... bei FEC-Korrektur skippen?
/*	if (!check_CRC_bits (data, packetLength * 8)) { // && Flag of Sessionheader ...
	      if (FEC_scheme != 1) { crcErrors ++; return; }
	} */
	
	// Initalize buffer arrays
	/*if (packetState == 0) {	
	      series. resize(0);
	      frameBytes. resize(0);
	      outVector. resize(0);
	}*/

	// CHECK: First packet of data group?
	if (firstLast == 2) { // do not check packetstate?!
	      //fprintf(stderr,"** DBG FIRST Data Group packet **\n");
              fprintf(stderr,"** DBG FLOW FIRST PS=%d PL=%d CI=%d FL=%d adr=%d cmd=%d UL=%d\n",packetState,packetLength,continuityIndex,firstLast,address,command,usefulLength);
//	      series. resize(0);
//	      frameBytes. resize(0);
	      handleFDGPacket (data, usefulLength);	/* NOTE Get CRC FLag?? */
	      packetState=1;
	      return;
	}
	// CHECK: The one and only packet?
	if (packetState == 0 && firstLast == 3) { 	// single packet, mostly padding
	      Packet2Arr(data, usefulLength);
	      fprintf(stderr,"** DBG Single Packet adr=%d=%d? len=%d 0x",address,packetAddress,usefulLength);
	      for (j=0 ; j < usefulLength; j++) { fprintf(stderr,"%02X",j,getBits(data,24+j*8,8)); } fprintf(stderr,"\n");
	      if (FEC_scheme != 1) my_dataHandler -> add_mscDatagroup (series);
	      packetState = 1; //2*FEC_scheme;
	      return;
	}
	// CHECK: Handle intermediate data 
	if (packetState == 1 && firstLast == 0) {	// handleIDGPacket (data, usefulLength);
	      //fprintf(stderr,"** DBG Intermediate Data Group Packet len=%d\n",usefulLength);
              fprintf(stderr,"** DBG FLOW INTER PS=%d PL=%d CI=%d FL=%d adr=%d cmd=%d UL=%d\n",packetState,packetLength,continuityIndex,firstLast,address,command,usefulLength);
	      Packet2Arr(data, usefulLength);
	      return;
	}
	// CHECK: Handle last packet
	if (packetState == 1 && firstLast == 1) {	// handleLDGPacket (data, usefulLength);
              fprintf(stderr,"** DBG FLOW LAST! PS=%d PL=%d CI=%d FL=%d adr=%d cmd=%d UL=%d series[%ld] frameBytes[%ld]\n"
                            ,packetState,packetLength,continuityIndex,firstLast,address,command,usefulLength,series.size(),frameBytes.size());
	      Packet2Arr(data, usefulLength);
	      if (FEC_scheme != 1) {
	      	    my_dataHandler -> add_mscDatagroup (series);
	      } // else { applyFEC(); } ???
	      packetState = 0; //2*FEC_scheme;
	      return;
	}
	fprintf(stderr,"** DBG FLOW ERROR PS=%d PL=%d CI=%d FL=%d adr=%d cmd=%d UL=%d\n",packetState,packetLength,continuityIndex,firstLast,address,command,usefulLength);
	return;
}
//	NOTE VERIFIED! -> CHECK
//      Handle sequence of FEC packets and RS decoding
void	dataProcessor::handleRSDF (uint8_t *data ) {
uint8_t 	temp     = 0;
uint16_t	rs_dt_pos= 0;
uint8_t Counter = getBits_4 (data, 2);
uint16_t PackAdr = getBits (data,7,10);
uint16_t	ii,i, j, k, rt_ds_pos;
int16_t currentLength = frameBytes. size ();
//int16_t	packetLength	= (getBits_2 (data, 0) + 1) * 24;	// 24-96 bytes
//int16_t	continuityIndex	= getBits_2 (data, 2);			// linking packages
//uint8_t	PL=0,UL=0;

	// Fill up application data table with padding zeros
//	fprintf(stderr,"** DBG: RS Packet #%d adr=%04X\n",Counter,PackAdr); 
	if (currentLength <= 2255) {	
		fprintf(stderr,"** DBG: RS Packet #%d; adding %d padding bytes for FEC!\n",Counter,2256+192 - currentLength);
		frameBytes. resize(2256+192-1);
		for (i = 1; i <= (frameBytes. size() - currentLength); i++) 
		      frameBytes[currentLength + i] = 0;
        } else if (currentLength > 2447) { fprintf(stderr,"!! DBG: Data Table Size > 2256 Bytes [%d]\n",currentLength); 
//	} else { fprintf(stderr,"** DBG: RS Packet #%d\n",Counter); 
	}

	// Populate Reed-Solomon Data Table
        for (i = 0; i < 22; i ++) {      
	      temp = 0; for (j = 0; j < 8; j ++) {
	            temp = (temp << 1) | (data [(i+2) * 8 + j] & 01); 	// V [i * 8 + j] & 01 -> Bitpuffer
	      } 
              rs_dt_pos = 188*12 + 22*Counter + i; if (rs_dt_pos < 2448) frameBytes [rs_dt_pos] = temp;	// skip 6 padding null bytes
	} 
	
	// EXIT, if still parts are missing ... paket-state?
	if (Counter < 8) { packetState=2; return; }
	applyFEC();
}

//
//      Apply reed solomon forward error correction
/*      (204, 188) Reed-Solomon code is a truncated version of RS(255, 239) code. It deletes the
	first 51 symbols of RS(255, 239) code since they are all zeros. Therefore, their design
	and realization methods are similar. RS(204, 188) code is defined on Galois field GF(2^8). */
void	dataProcessor::applyFEC (void) {
uint8_t         rsIn    [204]; // 120
uint8_t         rsOut   [188]; // 110 // 188
uint16_t	i,ii, j, k;
uint8_t		m=0;
int16_t		ler;
uint8_t	PL=0,UL=0;
uint8_t rse, base             = 0;

	// Apply forward error correction
	fprintf(stderr,"** DBG FEC Status = ");
        for (j = 0; j < RSDims; j ++) {
              for (k = 0; k < 204; k ++) {
//                    if (k < 188) { rsIn [k] =  outVector [(base + j + k * RSDims) % (RSDims * 204)]; 	// 120
//                    } else {        
		      rsIn [k] = frameBytes [(base + j + k * RSDims) % (RSDims * 204)]; 	// 120
	      } 
	      rse=(my_rsDecoder. dec( rsIn, rsOut, 51)<0)?1:0; rsErrors+=rse; //              rsErrors+=(my_rsDecoder. dec( rsIn, rsOut, 51)<0)?1:0; 						// mp4: 135 / 51? -> acc. 5.3.5.1
	      fprintf(stderr,"%d",rse);
  	      for (k = 0; k < 188; k ++) outVector [j + k * RSDims] = rsOut [k]; 				// Verified OutVector=frameBytes!
	} fprintf(stderr,"\n");

/*	// Extract MSC data group / copy without packet header and CRC
	fprintf(stderr,"*** DBG: Size of Series=%ld ",series.size());
	if (series.size() >= 200) for (i=0; i < 200; i++) fprintf(stderr,"%d",series[i]); */
	//series. resize(0);
	ii=0;  	//while ( ((frameBytes[ii]&0xC)>>2) != 1 && ii <= frameBytes.size() ) {

/*	for (ii = 0; ((frameBytes[ii]&0xC)>>2) == 1; ii+=24*(((frameBytes[ii]&0xC0)>>6)+1)) {
	      UL = frameBytes[ii+2]&0x7F;
	      if (UL > 0) {
	           fprintf(stderr,"** DBG i=%04d UL=%d\n",ii,UL);
	      }
	 } */
	 
	
/*	      PL = 24*(((frameBytes[ii]&0xC0)>>6)+1);
	         currentLength = series. size ();
	         fprintf(stderr,"i=%04d PL=%d UL=%d (bytes to add) series[%d] ",ii,PL,UL,currentLength);
	         for (j = 0; j <8; j++) fprintf(stderr,"%02X%s",frameBytes[ii+j],(j==4)?"|":"");
	         fprintf(stderr," ->");
                 series. resize(currentLength+UL*8);
                 fprintf(stderr,"series(%ld)\n",series.size());
   	         for (j = 0; j < (series.size()-currentLength); j++) {
//   	            fprintf(stderr,"CL+j=%d < %d  fb[%d] < %d\n",j,series.size(),ii+3+j/8,frameBytes.size());
	      	    uint8_t bit=j%8;
	      	    series[currentLength+j]=(frameBytes[ii+3+j/8]&(1<<(7-bit)))>>(7-bit);
	         }
	      }
//              ii+=24*(((frameBytes[ii]&0xC0)>>6)+1);
//              fprintf(stderr,"** I=%d\n",i);
	}
	fprintf(stderr,"*** DBG: Transferred %d Frame Bytes to %ld Series Bits.\n",ii,series.size());
	fprintf(stderr,"*** DBG: Size of Series=%ld ",series.size());
	if (series.size() >= 200) for (i=0; i < 200; i++) fprintf(stderr,"%d",series[i]);*/
//	if (rse == 0 ) {
		my_dataHandler -> add_mscDatagroup (series);
		series. resize(0);
		frameBytes.resize(0);
		//outVector.resize(0);
//	}
	packetState = 0;
	return;
}		

//
// Reed-Solomon Verification -> https://scholarworks.calstate.edu/downloads/vh53wz89h
/* (204, 188) REED-SOLOMON CODE ENCODER/DECODER DESIGN, SYNTHESIS, AND SIMULATION ...
   A graduate project submitted in partial fulfillment of the requirements 
   For the degree of Master of Science in Electrical Engineering
   By Haoyi Zhang
** DBG example data table for dt[0]=1, dt[1]=2 .. dt[187]=188
** DBG k=51? -> 0xc3e75ac28e7055ab3ff2fb9a015221de
** DBG k=040 -> 0xc3e75ac28e7055ab3ff2fb9a015221de
** DBG k=050 -> 0xc3e75ac28e7055ab3ff2fb9a015221de
** DBG k=051 -> 0xc3e75ac28e7055ab3ff2fb9a015221de
** DBG k=052 -> 0x1d78d6c860f878b7189f1a54961d5fde
** DBG k=053 -> 0x5b72fe577a4c082bb13c0532b7f85fde
** DBG k=054 -> 0xd368bd821fc277f7e5adae9cf2f85fde
** DBG k=055 -> 0xa30c3367c65747db6519333df2f85fde
** DBG k=056 -> 0xfa1fa6a9c8b94631f93a4b3df2f85fde
** DBG k=057 -> 0x56b678fca4ea7b69be0a4b3df2f85fde
** DBG k=058 -> 0xd6e19210dcfb645a5a0a4b3df2f85fde
** DBG k=059 -> 0x03084d8f451cd2cd5a0a4b3df2f85fde
** DBG k=060 -> 0x08b378e0ccad3fcd5a0a4b3df2f85fde
** DBG k=061 -> 0x83aa97dde0e13fcd5a0a4b3df2f85fde
** DBG k=062 -> 0x7b8b3dbed2e13fcd5a0a4b3df2f85fde
** DBG k=063 -> 0xabfd5078d2e13fcd5a0a4b3df2f85fde
** DBG k=064 -> 0x2fb49478d2e13fcd5a0a4b3df2f85fde
** DBG k=065 -> 0xaee09478d2e13fcd5a0a4b3df2f85fde
** DBG k=066 -> 0x86e09478d2e13fcd5a0a4b3df2f85fde
** DBG k=067 -> 0x86e09478d2e13fcd5a0a4b3df2f85fde
//
void	dataProcessor::RS_dry_run (void) {
uint8_t         rsIn    [204]; // 120
uint8_t         rsOut   [204]; // 110 // 188
uint16_t	i,ii, j, k;
uint8_t		m=0;
int16_t		ler;
uint8_t	PL=0,UL=0;
uint8_t rse, base             = 0;
uint8_t RSDC[16] = {195, 231, 90, 194, 142, 112, 85, 171, 63, 242, 251, 154, 1, 82, 33, 222};

	for (m=0; m<204 ;m++ ) { 
		if (m < 188) { 	rsIn[m]=m+1; 
		} else {       	rsIn[m]=RSDC[m-188]; } 
		rsOut[m]=0; 
	} 
	fprintf(stderr," m[1]=%d m[203]=%d\n",rsIn[1],rsIn[203]);
	fprintf(stderr,"!!!!! III\n");
		
	ler = my_rsDecoder. dec(rsIn, rsOut, 51);
	fprintf(stderr,"!!! RS CHK ler=%d ",ler);
	if ( ler <= 0 ) {
	      fprintf(stderr,"ERR\n");
	      fprintf(stderr,"** DBG example data table for dt[0]=%d, dt[1]=%d .. dt[187]=%d\n** DBG k=51? -> 0x",rsIn[0],rsIn[1],rsIn[187]);
	      for (k=0; k<16; k++) {fprintf(stderr,"%02x",RSDC[k]);} fprintf(stderr,"\n");
	      for (k=40; k<71; k++) {
	      	fprintf(stderr,"** DBG k=%03d -> 0x",k);
	        my_rsDecoder. enc(rsIn, rsOut, k); 
	        for (m = 188; m < 204; m ++) fprintf(stderr,"%02x",rsOut[m]);
	        fprintf(stderr,"\n");
              } 
	} else { fprintf(stderr,"OK\n"); }
...*/










//
// 	Handle first packet of data group
//	      for (i=0; i < 2448; i ++) frameBytes[i]=0;
void	dataProcessor::handleFDGPacket (uint8_t *data, int16_t length) {
uint16_t	offset=40;	         
// MSC Data Group header 
uint8_t	ExtensionFlag	= getBits_1 (data, 24); // 1 = extension field present
uint8_t	CRCFlag		= getBits_1 (data, 25); // 1 = CRC data group at end of DG
uint8_t	SegmentFlag	= getBits_1 (data, 26); // 1 = segment field present
uint8_t	UserAccessFlag	= getBits_1 (data, 27); // 1 = user access field present
uint8_t	DataGroupType	= getBits_4 (data, 28); // 0 = general data / 1 CA msg acc. ETSI TS 102 367
uint8_t	ContinuityFlag  = getBits_4 (data, 32); // inkrementiert ...
uint8_t	RepetitionFlag	= getBits_4 (data, 36); // 1111 = "Dauerloop"
int16_t	ExtensionField	= getBits (data, offset, 16 ); 
//	fprintf(stderr,"** MSC DG ExtFlg=%d CRC=%d SegFlg=%d UsrAcFlg=%d DGTy=%d CntFlg=%d RepFlg=%d",
//	      			ExtensionFlag, CRCFlag, SegmentFlag, UserAccessFlag, DataGroupType, ContinuityFlag, RepetitionFlag);
	(void)ExtensionFlag;
	(void)CRCFlag;
	(void)DataGroupType;
	(void)ContinuityFlag;
	(void)RepetitionFlag;
	(void)ExtensionField;
	
	packetState = 1;
	if (ExtensionFlag == 1) { 	// CA information on data group level
	      offset += 16;		//		 fprintf(stderr, " ExtField=%04X\n", ExtensionField);
	} 				// else { fprintf(stderr, "\n"); }

	/* Optional Session header (16 bits segments + (n+1) x 8 bits User access field )*/
	if (SegmentFlag == 1 && UserAccessFlag == 1) {	//	      	  fprintf(stderr,"** MSC Session header");
              //offset=56;
              if (SegmentFlag == 1) {
	            int8_t	LastFlag	= getBits_1 (data, offset);  // 0 .. there is more / 1 = last segment
		    int16_t     SegmentNumber   = getBits (data, offset+1,15); // starting form 0 ...
		    fprintf(stderr," Last=%d SegNr=%d",LastFlag,SegmentNumber);
		    offset += 16;
	       }
	       if (UserAccessFlag == 1) {
		    uint8_t	Rfa		= getBits_3  (data, offset); // reserverd for future
		    uint8_t	TransportIDFlag	= getBits_1  (data, offset+3); // 1 = transport ID field present
		    uint8_t	LengthIndicator = getBits_4  (data, offset+4); // Länge der TransportID und End-User Adressfeld
		    uint16_t	TransportId	= getBits (data, offset+8, 16); // Unique Object Identifier
		    uint16_t	EndUserAdrField = getBits (data, offset+24, 16); // kann auch größer sein!
		    fprintf(stderr, " Rfa=%d TransIDFlg=%d LI=%d TransId=%04X EUsrA=%04X", Rfa, TransportIDFlag, LengthIndicator, TransportId, EndUserAdrField);
		}
		fprintf(stderr,"\n");
	} // CntFlg einzig sinnvoll!
	Packet2Arr(data, length);
}

//
// 	Copy data buffer to series array 
void	dataProcessor::Packet2Arr (uint8_t *data, int16_t length) {
int32_t currentLength = series. size ();
uint16_t	i, j, pl;
uint8_t		temp=0;
uint8_t		debug=0;
uint16_t	packetLength	= (getBits_2 (data, 0) + 1) * 24;	// 24-96 bytes

//	crc = check_CRC_bits (data, packetLength * 8);
//	fprintf(stderr, "** DBG CRC=%d **\n",crc);

/*	fprintf(stderr,"** DBG CRC[%d]=",packetLength*8); if (!check_CRC_bits (data, packetLength * 8)) { // && Flag of Sessionheader ...
	      //if (FEC_scheme != 1) { crcErrors ++; return; }
	      fprintf(stderr,"FAIL\n");
//	      check_CRC_bits (data, packetLength * 8);
	      for (i = 0; i < packetLength; i++) {    
	          temp = 0; for (j = 0; j < 8; j ++) {
	            temp = (temp << 1) | (data [(i+0) * 8 + j] & 01); 	// V [i * 8 + j] & 01 -> Bitpuffer
	          }
	          fprintf(stderr,"%02x",temp);  
	      } fprintf(stderr,"\n");
	}  else { fprintf(stderr,"PASS\n"); }*/


	series. resize (currentLength + 8 * length);
	if (debug == 1) fprintf(stderr,"**!* DBG L=%d\n",length);
	if (debug == 1) { fprintf(stderr,"** DBG Cpy series[%04d]=0x",currentLength); }
	for (i = 0; i < 8 * length; i ++) {
	      series [currentLength + i] = data [24 + i];	//	      print_buffer_hex(series.data(), series.size());
	}
	if (debug == 1) { for (i = 0; i <= 10; i++) { fprintf(stderr,"%02X",getBits(data,24+i*8,8)); } }
	      
	// Fill FEC frameBytes buffer
	currentLength = frameBytes. size();
//	if (debug == 1) { 
//	fprintf(stderr,"** DBG frameBytes[%03d+%02d]=>",currentLength,length); 
	frameBytes. resize(currentLength + (3 + length + 2));	// 3 packet header byte + packet data + 2 packet CRC bytes
//	fprintf(stderr,"%d\n",frameBytes.size());
	
	for (i = 0; i < (3 + length + 2); i ++) {      		// packet bytes
	      temp = 0;
	      for (j = 0; j < 8; j ++) 
                    temp = (temp << 1) | (data [i * 8 + j] & 01); // V [i * 8 + j] & 01 -> Bitpuffer
	      frameBytes [currentLength + i] = temp;
	      if (debug == 1 && i <=10) fprintf(stderr,"%02X",temp);
	      if (debug == 1 && i == 2) fprintf(stderr,"|");
	} if (debug == 1) { 		fprintf(stderr,"\n");}
//	fprintf(stderr,"** DBG frameBytes[%ld]\n",frameBytes.size());
	return; // NOTE SKIP the moving BLOCK!
	fprintf(stderr,"!!!!! SHOULD NOT HAPPEN !!!!!\n");

	i = 0; while ((frameBytes.size()-i) > (188*12)) {
	      pl = 24*(((frameBytes[i]&0xC0)>>6)+1);
//	      fprintf(stderr,"** DBG Stripping %d bytes -> %ld\n",pl,frameBytes.size()-pl);
	      i+=pl;
	} 
	if (i > 0 ) {
	      i = 188*12;
	      fprintf(stderr,"** DBG Framesize now = %ld\n",frameBytes.size()-i);
	      outVector.resize(188*12-1,0);	//  outVector.resize(frameBytes.size()-i-1);
//	      for ( j = 0; j < (frameBytes.size()-i); j ++) {
	      for ( j = outVector.size(); j == 0; j --) {
	            outVector[j] = frameBytes[i+j];
	      }
	      fprintf(stderr,"** DBG outVector[%ld] ov[%d]=%02X fb[%d]=%02X **\n",outVector.size(),j,outVector[j],i+j,frameBytes[i+j]);
	} 
}
/*	for (ii = 0; ((frameBytes[ii]&0xC)>>2) == 1; ii+=24*(((frameBytes[ii]&0xC0)>>6)+1)) {
	      UL = frameBytes[ii+2]&0x7F;
	      if (UL > 0) {
	           fprintf(stderr,"** DBG i=%04d UL=%d\n",ii,UL);
	      } */


//int16_t currentLength = frameBytes. size ();
//       if (currentLength <= 2255) {





//
// 	Handle last packet of data group
//void	dataProcessor::handleLDGPacket (uint8_t *data, int16_t length) {
//uint16_t	i, j;
//int32_t currentLength = series. size ();
//uint8_t 	temp     = 0;
//	Packet2Arr(data, length);
//	my_dataHandler -> add_mscDatagroup (series);
//	series. resize(0);
//	packetState = 0;
//}


//
// 	Handle intermediate packets of data group
//void	dataProcessor::handleIDGPacket (uint8_t *data, int16_t length) {
//int32_t currentLength = series. size ();
//uint16_t	i, j;
//uint8_t 	temp     = 0;
//	series. resize (currentLength + 8 * length);		//	fprintf(stderr,"**** DBG Copy %d from data[] to series[%d...] ***",8*usefulLength,currentLength);
//	for (i = 0; i < 8 * length; i ++) 
//	      series [currentLength + i] = data [24 + i];

	// NOTE Fill FEC frameBytes buffer
/*	for (i = 0; i < packetLength; i ++) {      // in bytes
	      temp = 0;
	      for (j = 0; j < 8; j ++) {
                    temp = (temp << 1) | (data [i * 8 + j] & 01); // V [i * 8 + j] & 01 -> Bitpuffer
	      }
	      frameBytes [blockFillIndex * packetLength + i] = temp; 
	} */
//	fprintf(stderr,"and %d into frameBytes[].\n",packetLength);
//	blocksInBuffer++;
//	blockFillIndex= (blockFillIndex+1); // %5? .. to be adjusted
//}

//	series. resize (currentLength + 8 * length);
//	fprintf(stderr,"**** DBG Copy %d from data[] to series[%d...] ***",8*usefulLength,currentLength);
//	for (i = 0; i < 8 * length; i ++)
//	      series [currentLength + i] = data [24 + i];
//	      print_buffer_hex(series.data(), series.size());

	// NOTE FEC Support -> Initially fill
	/*for (i = 0; i < packetLength; i ++) {      // in bytes
	      temp = 0;
              for (j = 0; j < 8; j ++) {
                    temp = (temp << 1) | (data [i * 8 + j] & 01); // V [i * 8 + j] & 01 -> Bitpuffer
	      }
	      frameBytes [blockFillIndex * packetLength + i] = temp; 
//	      fprintf(stderr, "%02X ");
	} */
//	fprintf(stderr,"and %d into frameBytes[] total=%d bytes",packetLength,blockFillIndex*packetLength+i);
//	blocksInBuffer++;
//	blockFillIndex= (blockFillIndex+1); // %5? .. to be adjusted

//	fprintf(stderr, "** DBG: Got %ld data bytes idx=%d in %d blocks.\n",series.size(),currentLength+i,blocksInBuffer+1);
//	blocksInBuffer=0;
//        blocksInBuffer=0;
//        blockFillIndex=0;
// NOTE Hier wird das fertige Datenpaket übergeben.



	      /* NOTE Copy data-buffer to series array 		//	      fprintf(stderr,"**** DBG Copy %ld bits from data[] to series[0...] ",series.size());
	      for (i = 0; i < series. size (); i ++) {
	         series [i] = data [24 + i];  // 24+i = Bits 	//	         fprintf(stderr,"%d ",data[24+i]);
	      }*/
	         
	      /* NOTE FEC Support -> Initially fill
	      for (i = 0; i < packetLength; i ++) {      // in bytes
	         temp = 0;
        	 for (j = 0; j < 8; j ++) 
        	    temp = (temp << 1) | (data [i * 8 + j] & 01); // V [i * 8 + j] & 01 -> Bitpuffer//        	    fprintf(stderr,"%04d %02X ", blockFillIndex * packetLength + i, temp);
	         frameBytes [blockFillIndex * packetLength + i] = temp;
	      }	//	      fprintf(stderr,"and %d into frameBytes[].\n",packetLength);
	      blocksInBuffer++;
	      blockFillIndex= (blockFillIndex+1); // %5? .. to be adjusted
	   }*/



/*	while (true) {
	   int16_t pLength = (getBits_2 (data, 0) + 1) * 24 * 8;
           fprintf(stderr, "*!* DBG packet length mismatch %d!=%d appType=%d\n",length,pLength,appType);
	   
	   handlePacket (data);
	   length -= pLength;
	   fprintf(stderr,"*!* DBG length=%d\n",length);
	   if (length < 2) {
	      fprintf(stderr, "*** EXIT ***\n");
	      return; }
	   data	= &(data [pLength]);
	} */
//}
//
//	Handle a single DAB packet:
//	Note, although not yet encountered, the standard says that
//	there may be multiple streams, to be identified by
//	the address. For the time being we only handle a single
//	stream!!!!
//	see section 5.3.2 of EN 300 401
//void	dataProcessor::handlePacket (uint8_t *data) {
//	if (usefulLength > 0)
//	fprintf (stderr, "Len=%d/%d+,CI=%d,addr=0x%03X,dc=%d,fl=%s\n",usefulLength,packetLength,continuityIndex,
//		address,command,getPacketType(firstLast));
//	fprintf (stderr, "CI = %d, addr = %d, paddr=%d, usefulLength = %d\n",
//	                 continuityIndex, address, packetAddress, usefulLength);

//	(void)continuityIndex;
//	(void)command;
//	   fprintf(stderr, "*!*!* DBG FOUND FEC packet l=%d, cnt=%02d adr=%d *!*!*!*\n", packetLength, Counter, address);
//	   fprintf(stderr, "***** blkinbuf=%d blkFillIdx=%d\n",blocksInBuffer,blockFillIndex);
	   /* NOTE RS Data Field Mapping Cnt x 22 + i */
           // NOTE FEC Support -> Initially fill
	   // NOTE EXIT FEC
//           for (i=0; i < 2448; i ++) frameBytes[i]=0;
//	   return;
//	}
//	fprintf(stderr,"*** DBG: adr=%d FL=%s CI=%d\n",address,getPacketType(firstLast),continuityIndex);
	
//	assemble the full MSC datagroup
// NOTE Hier werden die Pakete verarbeitet .. syncing? mit erstem Paketbyte?
/* 	Packets (as defined in clause 5.3.2) are fed into the Application Data Table column by
        column starting with the first byte of the first packet going into row 1, column 1 and
        moving downwards row by row and to the right, column by column. 
        Since there are 12 rows, each 24 byte, 48 byte, 72 byte or 96 byte packet exactly fills
        2 columns, 4 columns, 6 columns or 8 columns.  */
/*        
	   else
	      series. resize (0);	// packetState remains 0
	}
	   else
	   if (firstLast == 01) {	// last packet
// NOTE LAST PACKET
	   else
	   if (firstLast == 02) {	// first packet, previous one erroneous
	      packetState = 1;
	      series. resize (usefulLength * 8);
//	      fprintf(stderr,"*!!* DBG Copy %ld from data[] to series[0...] ***\n",series.size());
	      for (i = 0; i < series. size (); i ++)
	         series [i] = data [24 + i];
	   }
	   else {
	      packetState = 0;
	      series. resize (0);
	   }
	}
}*/
//
//
//	Really no idea what to do here
void	dataProcessor::handleTDCAsyncstream (uint8_t *data, int16_t length) {
int16_t	packetLength	= (getBits_2 (data, 0) + 1) * 24;
int16_t	continuityIndex	= getBits_2 (data, 2);
int16_t	firstLast	= getBits_2 (data, 4);
int16_t	address		= getBits   (data, 6, 10);
uint16_t command	= getBits_1 (data, 16);
int16_t	usefulLength	= getBits_7 (data, 17);

	(void)	length;
	(void)	packetLength;
	(void)	continuityIndex;
	(void)	firstLast;
	(void)	address;
	(void)	command;
	(void)	usefulLength;
	fprintf(stderr, "DP: Clueless\n");
	if (!check_CRC_bits (data, packetLength * 8))
	   return;
}

