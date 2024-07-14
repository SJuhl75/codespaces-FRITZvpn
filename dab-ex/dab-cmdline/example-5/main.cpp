/*
 *    Copyright (C) 2015, 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the DAB-library
 *
 *    DAB-library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    DAB-library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DAB-library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	E X A M P L E  P R O G R A M
 *	for the DAB-library

ZM001 – ZM010 are SSR correction messages (mostly updated in seconds) while the subsequent message types
ZM011++       contain the SSRZ Metadata. 
SSRZ Metadata will be available via text file, internet, or they are broadcast as separate messages or split 
and piggybacked on the correction message stream.

*/
#include	<unistd.h>
#include	<signal.h>
#include	<getopt.h>
#include        <cstdio>
#include        <cstdint>
#include 	<cstdarg>
#include	<algorithm>
#include 	<curses.h>

#include        <iostream>
#include	<complex>
#include	<vector>
#include	"audiosink.h"
#include	"filesink.h"
#include	"dab-api.h"
#include	"locking-queue.h"
#include	"includes/support/band-handler.h"
#ifdef	HAVE_SDRPLAY
#include	"sdrplay-handler.h"
#elif	HAVE_AIRSPY
#include	"airspy-handler.h"
#elif	HAVE_RTLSDR
#include	"rtlsdr-handler.h"
#elif	HAVE_WAVFILES
#include	"wavfiles.h"
#elif	HAVE_RAWFILES
#include	"rawfiles.h"
#elif	HAVE_RTL_TCP
#include	"rtl_tcp-client.h"
#endif

#include	<atomic>
#include	<thread>
#ifdef	DATA_STREAMER
#include	"tcp-server.h"
#endif

using std::cerr;
using std::endl;
//
//      messages
typedef struct {
   int key;
   std::string string;
} message;

static
lockingQueue<message> messageQueue;

#define	S_QUIT	0100
#define	S_NEXT	0101
#define	S_TMNG	0102

#define MAX_BUFFER_SIZE 4096 // Adjust the buffer size accordingly

// Declare the function prototype
void 		procZM011(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM012(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM013(const std::vector<bool>& bitArray, size_t& bitidx);
int_fast64_t	rdZDB001(const std::vector<bool>& bitArray, size_t& bitidx, size_t binsize);
void 		procZMB001(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZMB002(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB003(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB004(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB005(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB006(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB007(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB008(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB011(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB013(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);
void 		procZMB025(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020);

void 		procZM001(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM002(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM003(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM004(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM005(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM006(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM007(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM008(const std::vector<bool>& bitArray, size_t& bitidx);
void 		procZM009(const std::vector<bool>& bitArray, size_t& bitidx);

void 		procZDF050(const std::vector<bool>& bitArray, size_t& bitidx, int mtnf);

void    printOptions	(void);	// forward declaration
void	listener	(void);
void	selectNext	(void);
//	we deal with some callbacks, so we have some data that needs
//	to be accessed from global contexts
static
std::atomic<bool> run;

static
void	*theRadio	= NULL;

static
std::atomic<bool>timeSynced;

static
std::atomic<bool>timesyncSet;

static
std::atomic<bool>ensembleRecognized;

static
audioBase	*soundOut	= NULL;

#ifdef	DATA_STREAMER
tcpServer	tdcServer (8888);
tcpServer	dbgServer (8887);
#endif

//std::string	programName		= "Sky Radio";
std::string	programName		= "PPP-RTK-AdV";
//int32_t		serviceIdentifier	= -1;

/* NOTE Necessary libraries */
#include <map>
#include <chrono>
#include <ctime>
#include <bitset>

/* NOTE Define a type for the parameter tuple  */
typedef std::tuple<int, int, int, std::string> ParameterTuple;
// Define a structure to store the decoded value and timestamp
struct SSRValue {
    int value;
    std::chrono::system_clock::time_point timestamp;
};

// Resize collection (and initialize if necessary)
template<typename Collection>
void resizeCollection(Collection* collection) {
    // Define ValueType based on the type of elements stored in data
    using ValueType = typename std::remove_pointer<decltype(collection->data)>::type;

    // Allocate memory for one element if collection->data is null, or one additional element otherwise
    collection->data = reinterpret_cast<ValueType*>(
        collection->data ? realloc(collection->data, (collection->capacity + 1) * sizeof(ValueType)) :
                           malloc(sizeof(ValueType))
    );

    if (!collection->data) { // Handle memory allocation failure
        std::cerr << "Memory allocation failed." << std::endl;
        exit(EXIT_FAILURE); // Log and terminate
    }

    // Increment capacity if memory allocation is successful
    collection->capacity = collection->data ? collection->capacity + 1 : 1;
    fprintf(stderr,"\nResZ size=%ld cap=%ld \n",collection->size,collection->capacity);
}




// Template for Collection
struct timblk_t {		// Satellite Timing Group
    uint16_t bitmask;		// Sat Group List Bit Mask   -> ZDF016
    uint8_t length;		// Length of Update Interval -> ZDF053 uint6
    uint8_t offset;		// Offset of Update Interval -> ZDF054 uint6
    std::chrono::system_clock::time_point timestamp; // Updated
};

struct clkblk_t {
/*    uint8_t	// High Rate Clock Default Resolution 	dx0	ZDF060
    uint8_t	// Number of High Rate Clock Rice Block NRB_clk ZDF060
    uint8_t	// SatPar Rice Block Def for HR clk             ZDB021
    uint8_t	// bitmask	ZDF012
    uint8_t	// Default Bin Size Parameter	ZDF043 */
    uint16_t bitmask;		// Sat Group List Bit Mask            -> ZDB021->ZDF012
    uint32_t defres;		// High Rate Clock Default Resolution -> ZDF060
    uint32_t defbin;		// High Rate Clock Default Bin Size   -> ZDB021->ZDF043
    std::chrono::system_clock::time_point timestamp; // Updated
    // C0,C1,CHR,t0res,bin
};

// Structure to represent the dynamic collection
struct timblk_c {
    struct timblk_t* data;
    size_t size;
    size_t capacity;
};

struct hrcb_c {
    struct clkblk_t* data;
    size_t size;
    size_t capacity;
};

// GLOBAL Variables
struct  timblk_c SatTimGrp;
struct  hrcb_c   HRClk;

/* Initialize the collection
void InitSTG(struct timblk_c* collection) {
    collection->data = NULL;
    collection->size = 0;
    collection->capacity = 0;
}

void InitHRClk(struct hrcb_c* collection) {
    collection->data = NULL;
    collection->size = 0;
    collection->capacity = 0;
}*/

// Clean up the collection
void cleanupTC(struct timblk_c* collection) {
    free(collection->data);
    collection->size = 0;
    collection->capacity = 0;
}

template<typename Collection> // requires set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17 -g") in CMakeLists.txt
void UpdColl(Collection* collection, uint16_t bitmask, 
                      uint64_t par1 = 0, uint64_t par2 = 0,
                      uint64_t par3 = 0, uint64_t par4 = 0) {
    fprintf(stderr,"**** bm=%d par1=%ld par=%ld ***", bitmask, par1, par2);
    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
    bool found = false;
    size_t i;

    // Search for existing data record
    for (i = 0; i < collection->size; ++i) {
        if (collection->data[i].bitmask == bitmask) {
            // Update existing record
            found = true;
            break;
        }
    }

    if (!found) {
    	if (collection->size >= collection->capacity) {
    	fprintf(stderr,"***** s=%d c=%d ****",collection->size,collection->capacity);
        // Resize the collection if it's full
        resizeCollection(collection);
        i = collection->capacity; // Update i after resizing
        collection->data[i].bitmask = bitmask;
        }
    }

    // Update record
    fprintf(stderr,"**** UDATE i=%d p1=%d p2=%d ***",i,(par1 <= UINT8_MAX) ? static_cast<uint8_t>(par1) : 0,(par2 <= UINT8_MAX) ? static_cast<uint8_t>(par2) : 0);
    collection->data[i].timestamp = currentTime;

    // Assign parameters based on the type of collection
    using ValueType = typename std::remove_pointer<decltype(collection->data)>::type;
    if constexpr (std::is_same_v<ValueType, timblk_t>) {
        collection->data[i].length = (par1 <= UINT8_MAX) ? static_cast<uint8_t>(par1) : 0;
        collection->data[i].offset = (par2 <= UINT8_MAX) ? static_cast<uint8_t>(par2) : 0;
    } else if constexpr (std::is_same_v<ValueType, clkblk_t>) {
        collection->data[i].defres = (par1 <= UINT32_MAX) ? static_cast<uint32_t>(par1) : 0;
        collection->data[i].defbin = (par2 <= UINT32_MAX) ? static_cast<uint32_t>(par2) : 0;
    }

    // Increment the size of the collection
    collection->size++;
}


// Function to update or add a TimBlk record in the collection
void UpdSTG(struct timblk_c* collection, uint16_t bitmask, uint8_t length, uint8_t offset) {
    //const TimCollection* collection ??
    fprintf(stderr,"**!!**!! bit=%d len=%d off=%d !!**!!**",bitmask,length,offset);
    // Check if the record with the given bitmask already exists
    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
    for (size_t i = 0; i < collection->size; ++i) {
        if (collection->data[i].bitmask == bitmask) {
            // Update existing record
            collection->data[i].timestamp = currentTime; // std::chrono::system_clock::now(); // timestamp;
            collection->data[i].length = length;
            collection->data[i].offset = offset;
            return; // Exit the function
        }
    }
    // If the record does not exist, add a new one
    if (collection->size >= collection->capacity) {
        // Resize the collection if it's full
        resizeCollection(collection);
    }

    collection->data[collection->size].bitmask = bitmask;
    collection->data[collection->size].timestamp = currentTime; //std::chrono::system_clock::now(); // timestamp;
    collection->data[collection->size].length = length;
    collection->data[collection->size].offset = offset;
    // Increment the size of the collection
    collection->size++;
}

void UpdHRCB(struct hrcb_c* collection, uint16_t bitmask, uint32_t defres, uint32_t defbin) {
    // Check if the record with the given bitmask already exists
    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
    for (size_t i = 0; i < collection->size; ++i) {
        if (collection->data[i].bitmask == bitmask) {
            // Update existing record
            collection->data[i].timestamp = currentTime; // std::chrono::system_clock::now(); // timestamp;
            collection->data[i].defres = defres;
            collection->data[i].defbin = defbin;
            return; // Exit the function
        }
    }
    // If the record does not exist, add a new one
    if (collection->size >= collection->capacity) {
        // Resize the collection if it's full
        resizeCollection(collection);
    }

    collection->data[collection->size].bitmask = bitmask;
    collection->data[collection->size].timestamp = currentTime; //std::chrono::system_clock::now(); // timestamp;
    collection->data[collection->size].defres = defres;
    collection->data[collection->size].defbin = defbin;
    // Increment the size of the collection
    collection->size++;
}

/*
void displayTC(struct timblk_c* collection, WINDOW* win) {
    mvwprintw(win, 0, 5, "|SSRZ Timing Block|");
    mvwprintw(win, 1, 1, "Bitmask\t|Length\t|Offset\t|Timestamp\n");
    for (size_t i = 0; i < collection->size; ++i) {
        mvwprintw(win, 2+i,1, "%04X\t|%d\t|%d\t|%d\n", collection->data[i].bitmask,
                collection->data[i].length, collection->data[i].offset,
                static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                    collection->data[i].timestamp.time_since_epoch()).count()));
    }
    wrefresh(win);
}

void displayTCS(struct timblk_c* collection, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "|SSRZ Timing Block|\nBitmask\t|Length\t|Offset\t|Timestamp\n");

    for (size_t i = 0; i < collection->size; ++i) {
        char temp_buffer[256]; // Temporary buffer for each line
        snprintf(temp_buffer, sizeof(temp_buffer), "%04X\t|%d\t|%d\t|%d\n", collection->data[i].bitmask,
                 collection->data[i].length, collection->data[i].offset,
                 static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                     collection->data[i].timestamp.time_since_epoch()).count()));

        // Concatenate the temporary buffer to the main buffer
        strncat(buffer, temp_buffer, buffer_size - strlen(buffer) - 1);
    }
}
 |SSRZ Timing Block|
Bitmask	        |Length	|Offset	|Timestamp
0000000100100100|11	|6	|0.000162 */

void OutSTG(struct timblk_c* collection) {
    const char* filename = "/tmp/STG.log";  // Define the filename here

    // Open the file in write mode (creates the file if it doesn't exist, truncates it otherwise)
    FILE* file = fopen(filename, "w");

    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Write the header to the file
    fprintf(file, "|SSRZ Timing Block|\nBitmask         |Length\t|Offset\t|Timestamp\n");

    for (size_t i = 0; i < collection->size; ++i) {
    	// Convert bitmask to binary string
        char bitmaskBinary[17];
        for (int j = 15; j >= 0; --j) {
            bitmaskBinary[15 - j] = ((collection->data[i].bitmask >> j) & 1) ? '1' : '0';
        }
        bitmaskBinary[16] = '\0';
        
        // Calculate the time difference
	std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
        std::chrono::system_clock::time_point retrVal     = collection->data[i].timestamp;
	std::chrono::duration<double> deltaa = currentTime - retrVal;
	//collection->data[i].timestamp; fprintf(stderr,"********* DELTA %f %f",deltaa,deltaa.count());
	//std::cout << "Delta: " << deltaa.count() << " seconds" << std::endl;

        // Write each entry to the file
        fprintf(file, "%s|%d\t|%d\t|%f\n",
                bitmaskBinary,
                collection->data[i].length,
                collection->data[i].offset,
                deltaa.count()
                );
    }

    // Close the file
    fclose(file);
}

void OutHRCB(struct hrcb_c* collection) {
    const char* filename = "/tmp/HRCB.log";  // Define the filename here

    // Open the file in write mode (creates the file if it doesn't exist, truncates it otherwise)
    FILE* file = fopen(filename, "w");

    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Write the header to the file
    fprintf(file, "|SSRZ High Rate Clock Block|\nBitmask         |defres\t|defbin\t|Timestamp\n");

    for (size_t i = 0; i < collection->size; ++i) {
    	// Convert bitmask to binary string
        char bitmaskBinary[17];
        for (int j = 15; j >= 0; --j) {
            bitmaskBinary[15 - j] = ((collection->data[i].bitmask >> j) & 1) ? '1' : '0';
        }
        bitmaskBinary[16] = '\0';
        
        // Calculate the time difference
	std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
        std::chrono::system_clock::time_point retrVal     = collection->data[i].timestamp;
	std::chrono::duration<double> deltaa = currentTime - retrVal;
	//collection->data[i].timestamp; fprintf(stderr,"********* DELTA %f %f",deltaa,deltaa.count());
	//std::cout << "Delta: " << deltaa.count() << " seconds" << std::endl;

        // Write each entry to the file
        fprintf(file, "%s|%d\t|%d\t|%f\n",
                bitmaskBinary,
                collection->data[i].defres,
                collection->data[i].defbin,
                deltaa.count()
                );
    }

    // Close the file
    fclose(file);
}


//  Compressed Satellite Parameter Block
//struct SatPar {
	// M GNSS
	// M dx0 Default Resolution
	// M p0  Default Bin Size Parameter
	// C a   Scale Factor Indicator
	// C b   Bin Size Indicator
	// C ... Rice encoded data
//}

/* NOTE Define a map to store the default parameters */
std::map<int, ParameterTuple> ZDFP;
std::map<int, SSRValue> SSRdata;

/* NOTE Function declarations */
std::vector<bool> byteArr2Bits(const uint8_t, size_t);
int getIoB(const std::vector<bool>& bitArray, size_t& index, int zdf, int N0, int db, int);

/* Convert Intenger to float */
union Converter {
    int32_t intValue;
    float floatValue;
};

float i2f(int32_t bits) {
    union Converter converter;
    converter.intValue = bits;
    return converter.floatValue;
}

int cntBits(int n) {
    int count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

/* Function to populate the default parameters map for
   SSRZ Data Field Definitions Map
   Based on http://www.geopp.de/wp-content/uploads/2022/11/gpp_ssrz_v1_1_2.pdf
           https://www.geopp.de/pdf/gpp_ssrz_v1_1_2.pdf
*/
void InitZDFP() {
    // SSRZ Data Fields
    ZDFP[  2] = std::make_tuple( 2, 1, 5, "Message Type Number Indicator");
    ZDFP[  3] = std::make_tuple( 8, 0, 0, "Metadata Type Number");
    ZDFP[  4] = std::make_tuple( 0,-1, 0, "Message ID Bit Mask");
    ZDFP[  5] = std::make_tuple( 2,-1, 0, "Metadata IOD");
    ZDFP[  6] = std::make_tuple( 1, 0, 0, "Metadata Announcement Bit");
    ZDFP[ 10] = std::make_tuple( 5,-1, 0, "Number of Low Rate Satellite Groups");
    ZDFP[ 11] = std::make_tuple( 5,-1, 0, "Number of High Rate Satellite Groups");
    ZDFP[ 12] = std::make_tuple(16,-1, 0, "GNSS ID Bit Mask");
    ZDFP[ 13] = std::make_tuple( 7,-1, 0, "Maximum Satellite ID per GNSS and Group");
    ZDFP[ 14] = std::make_tuple( 4,-1, 0, "Satellite Group Definition Mode");
    ZDFP[ 15] = std::make_tuple( 0,-1, 0, "Satellite Group Bit Mask per GNSS");
    ZDFP[ 16] = std::make_tuple( 0,-1, 0, "SSRZ Satellite Group List Bit Mask");
    ZDFP[ 17] = std::make_tuple( 0,-1, 0, "Satellite Bit Mask per GNSS");
    ZDFP[ 18] = std::make_tuple( 0,-1, 0, "Satellite Bit Mask");
    ZDFP[ 19] = std::make_tuple(32,-1, 0, "SSRZ Signal Bit Mask per GNSS");
    ZDFP[ 20] = std::make_tuple( 8, 0, 0, "SSRZ Metadata Tag");
    ZDFP[ 21] = std::make_tuple(13,-1, 0, "Size of specific SSRZ Metadata Message Block");
    ZDFP[ 24] = std::make_tuple( 5, 0, 0, "SSRZ Number of Satellite Groups");
    ZDFP[ 25] = std::make_tuple( 8, 0, 0, "SSRZ Grid ID");
    ZDFP[ 26] = std::make_tuple( 0,-1, 0, "SSRZ Grid ID Bit Mask");
    ZDFP[ 27] = std::make_tuple( 8, 0, 0, "SSRZ Region ID");
    ZDFP[ 28] = std::make_tuple( 0,-1, 0, "SSRZ Region ID Bit Mask");
    ZDFP[ 29] = std::make_tuple( 1,-1, 0, "SSRZ Multi-Grid flag");
    ZDFP[ 30] = std::make_tuple( 1,-1, 0, "SSRZ Multi-Region flag");
    ZDFP[ 42] = std::make_tuple( 2, 1, 5, "Number of SSRZ Rice Blocks");
    ZDFP[ 43] = std::make_tuple( 2, 1, 5, "Default Bin Size Parameter");
    ZDFP[ 44] = std::make_tuple( 2,-1, 0, "Number of components per SSR parameter type");
    ZDFP[ 45] = std::make_tuple( 2, 0, 0, "Number of SSRZ Signal Rice Blocks");
    ZDFP[ 46] = std::make_tuple( 2, 0, 0, "Default Bin Size Parameter of encoded Signal Bias");
    ZDFP[ 47] = std::make_tuple( 0,-1, 0, "SSRZ Parameter List Bit Mask");
    // SSRZ Timing Data Fields
    ZDFP[ 50] = std::make_tuple(10,-1, 0, "SSRZ 15 minutes Time Tag");
    ZDFP[ 51] = std::make_tuple(12,-1, 0, "GPS week number");
    ZDFP[ 52] = std::make_tuple(20,-1, 0, "GPS epoch time 1s");
    ZDFP[ 53] = std::make_tuple( 6,-1, 0, "Length of SSR Update Interval");
    ZDFP[ 54] = std::make_tuple( 6,-1, 0, "SSR Offset Interval");
    ZDFP[ 55] = std::make_tuple( 5,-1, 0, "Number of Satellite dependent Timing Blocks");
    ZDFP[ 56] = std::make_tuple( 6, 0, 0, "SSR Length of SSR Update Interval");
    ZDFP[ 57] = std::make_tuple( 6, 0, 0, "SSR Update Interval Offset");
    ZDFP[ 58] = std::make_tuple( 3, 0, 0, "Number of Update and Offset Blocks");
    // SSRZ Default Resolutions (The final resolution settings are TBD)
    ZDFP[ 60] = std::make_tuple(10, 0, 0, "SSRZ High Rate Clock Default Resolution");
    ZDFP[ 61] = std::make_tuple(10, 0, 0, "SSRZ High Rate Radial Orbit Default Resolution");
    ZDFP[ 62] = std::make_tuple(10, 0, 0, "SSRZ Low Rate Clock 𝐶0  Default Resolution");
    ZDFP[ 63] = std::make_tuple(10, 0, 0, "SSRZ Low Rate Clock 𝐶1  Default Resolution");
    ZDFP[ 64] = std::make_tuple(10, 0, 0, "SSRZ Low Rate Radial Orbit Default Resolution");
    ZDFP[ 65] = std::make_tuple( 8, 0, 0, "SSRZ Low Rate Along-Track Orbit Default Resolution");
    ZDFP[ 66] = std::make_tuple( 8, 0, 0, "SSRZ Low Rate Cross-Track Orbit Default Resolution");
    ZDFP[ 67] = std::make_tuple(10, 0, 0, "SSRZ Low Rate Radial Velocity Default Resolution");
    ZDFP[ 68] = std::make_tuple(10, 0, 0, "SSRZ Low Rate Along-Track Velocity Default Resolution");
    ZDFP[ 69] = std::make_tuple(10, 0, 0, "SSRZ Low Rate Cross-Track Velocity Default Resolution");
    ZDFP[ 70] = std::make_tuple(11, 0, 0, "SSRZ Low Rate Code Bias Default Resolution");
    ZDFP[ 71] = std::make_tuple( 5, 0, 0, "SSRZ Low Rate Phase Bias Cycle Range");
    ZDFP[ 72] = std::make_tuple( 5, 0, 0, "SSRZ Low Rate Phase Bias Bitfield Length");
    ZDFP[ 73] = std::make_tuple( 5, 0, 0, "SSRZ Maximum Number of Continuity/Overflow bits");
    ZDFP[ 74] = std::make_tuple( 4, 0, 0, "SSRZ Default Resolution of the Satellite-dependent Global Ionosphere corrections");
    ZDFP[ 75] = std::make_tuple( 4, 0, 0, "SSRZ Gridded Ionosphere Correction Default Resolution"); 
    ZDFP[ 76] = std::make_tuple( 8, 0, 0, "SSRZ Gridded Troposphere Scale Factor Default Resolution");
    ZDFP[ 77] = std::make_tuple( 0, 0, 0, "SSRZ Gridded Troposphere Gradient Default Resolution");
    ZDFP[ 80] = std::make_tuple( 4, 0, 0, "SSRZ Default Resolution of the Satellite-dependent Regional Ionosphere Coefficients");
    ZDFP[ 81] = std::make_tuple( 4, 0, 0, "SSRZ Default Resolution of Global VTEC Ionosphere Coefficients");
    ZDFP[ 82] = std::make_tuple( 8, 0, 0, "SSRZ Default Resolution of Global VTEC Ionosphere Coefficients");
    ZDFP[ 85] = std::make_tuple( 8, 0, 0, "SSRZ Default Resolution of the Regional");
    ZDFP[ 86] = std::make_tuple( 8, 0, 0, "SSRZ Default Resolution of Regional Troposphere Mapping Improvements");
    ZDFP[ 89] = std::make_tuple( 7, 0, 0, "SSRZ QIX Code Bias Default Resolution");
    ZDFP[ 90] = std::make_tuple( 7, 0, 0, "SSRZ QIX Phase Bias Default Resolution");
    // SSRZ Grid Definition Data Fields
    ZDFP[ 91] = std::make_tuple( 4, 0, 0, "Number of Grids");
    ZDFP[ 92] = std::make_tuple( 2, 1, 0, "Order indicator of the grid point coordinate resolution");
    ZDFP[ 93] = std::make_tuple( 8,-1, 0, "Integer part of the coordinate resolution");
    ZDFP[ 94] = std::make_tuple( 4, 0, 0, "Number of Chains");
    ZDFP[ 95] = std::make_tuple( 5, 0, 0, "Number of Grid Points per Chain");
    ZDFP[ 96] = std::make_tuple( 4, 0, 0, "Grid Bin Size Parameter");
    ZDFP[ 97] = std::make_tuple( 1,-1, 0, "Use Baseline Flag");
    ZDFP[ 98] = std::make_tuple( 1,-1, 0, "Point Position Flag");
    ZDFP[ 99] = std::make_tuple( 1,-1, 0, "Add Baseline Left Flag");
    ZDFP[100] = std::make_tuple( 1,-1, 0, "Add Baseline Right Flag");
    ZDFP[101] = std::make_tuple( 1,-1, 0, "Height Flag");
    ZDFP[102] = std::make_tuple( 5, 0, 0, "Grid Point Height Resolution");
    ZDFP[103] = std::make_tuple( 1,-1, 0, "Gridded Data Predictor Points Flag");
    ZDFP[104] = std::make_tuple( 2, 0, 0, "Predictor Point Indicator");
    ZDFP[105] = std::make_tuple( 4, 0, 0, "Grid ID");
    ZDFP[106] = std::make_tuple( 3,-1, 0, "Grid IOD");
    // SSRZ Specific Troposphere Component Data Fields
    ZDFP[107] = std::make_tuple(16,-1, 0, "Troposphere Basic Component Bit Mask");
    ZDFP[108] = std::make_tuple( 1,-1, 0, "Separated Compressed Coefficient Blocks per Height Order Flag");
    // SSRZ Model Parameter Data Fields
    ZDFP[110] = std::make_tuple( 8,-1, 0, "Model ID");
    ZDFP[111] = std::make_tuple( 8,-1, 0, "Model Version");
    ZDFP[112] = std::make_tuple( 8,-1, 0, "Number of Integer Model Parameters");
    ZDFP[113] = std::make_tuple( 8,-1, 0, "Number of Float Model Parameters");
    ZDFP[114] = std::make_tuple(32,-1, 0, "Integer Model Parameter");
    ZDFP[115] = std::make_tuple(32,-1, 0, "Float Model Parameter"); // float32 Float value representation according to IEEE-754 !
    ZDFP[116] = std::make_tuple( 0,-1, 0, "Coefficient Order Bit Mask");
    // SSRZ Global VTEC Metadata Data Fields
    ZDFP[129] = std::make_tuple( 2, 1, 0, "SSRZ Encoder Type");
    ZDFP[130] = std::make_tuple( 2, 0, 0, "Number of Ionospheric Layers");
    ZDFP[131] = std::make_tuple(10, 0, 0, "Height of Ionospheric Layer");
    ZDFP[132] = std::make_tuple( 6, 0, 0, "Spherical Harmonics Degree");
    ZDFP[133] = std::make_tuple( 6, 0, 0, "Spherical Harmonics Order");
    // SSRZ Satellite-dependent Global Ionosphere Metadata Data Fields
    ZDFP[134] = std::make_tuple( 1,-1, 0, "Satellite-dependent Global Ionosphere Correction Flag");
    ZDFP[135] = std::make_tuple( 0, 0, 0, "GSI polynomial type");
    ZDFP[140] = std::make_tuple( 0, 0, 0, "Height of GSI Ionospheric Layer");
    // SSRZ Satellite-dependent Regional Ionosphere Metadata Data Fields
    ZDFP[143] = std::make_tuple( 4, 0, 0, "Number of Regions");
    ZDFP[144] = std::make_tuple( 4, 0, 0, "Region ID");
    ZDFP[145] = std::make_tuple( 0, 0, 0, "RSI polynomial Type");
    ZDFP[147] = std::make_tuple(10, 0, 0, "Height of RSI Ionospheric Layer");
    ZDFP[148] = std::make_tuple(11,-1, 0, "Latitude of RSI Ground Point Origin");
    ZDFP[149] = std::make_tuple(12,-1, 0, "Longitude of RSI Ground Point Origin");
    ZDFP[150] = std::make_tuple(14,-1, 0, "Height of RSI Ground Point Origin");
    // SSRZ Regional Troposphere Correction Metadata Data Fields
    ZDFP[166] = std::make_tuple( 2, 0, 0, "Maximum Horizontal Order");
    ZDFP[167] = std::make_tuple( 2, 0, 0, "Maximum Vertical Order");
    ZDFP[168] = std::make_tuple(11,-1, 0, "Latitude of regional troposphere Ground Point Origin");
    ZDFP[169] = std::make_tuple(12,-1, 0, "Longitude of regional troposphere Ground Point Origin");
    ZDFP[170] = std::make_tuple(14,-1, 0, "Height of regional troposphere Ground Point Origin");
    ZDFP[171] = std::make_tuple( 6,-1, 0, "Maximum Elevation for Mapping Improvement");
    ZDFP[172] = std::make_tuple( 8, 0, 0, "Coverage Dependent Factor");
    ZDFP[173] = std::make_tuple( 8, 0, 0, "Vertical Scale Factor");
    // SSRZ QIX Bias metadata fields
    ZDFP[190] = std::make_tuple( 1,-1, 0, "QIX Code Bias flag");
    ZDFP[191] = std::make_tuple( 1,-1, 0, "QIX Phase Bias flag");
    ZDFP[199] = std::make_tuple( 4,-1, 0, "Length of the SSRZ BE IOD bit field");
    ZDFP[200] = std::make_tuple( 2, 0, 0, "SSRZ IOD Tag");
    ZDFP[201] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag GPS");
    ZDFP[202] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag GLONASS");
    ZDFP[203] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag Galileo");
    ZDFP[204] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag QZSS");
    ZDFP[205] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag SBAS");
    ZDFP[206] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag BDS");
    ZDFP[207] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag IRNSS");
    ZDFP[208] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[209] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[210] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[211] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[212] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[213] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[214] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[215] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    ZDFP[216] = std::make_tuple( 2, 0, 0, "SSRZ BE IOD Tag");
    // SSRZ Time Tag Message specific Data Fields
    ZDFP[230] = std::make_tuple( 2, 1, 0, "SSRZ Time Tag Definition");
    ZDFP[231] = std::make_tuple( 7,-1, 0, "SSRZ 1hour- 30seconds Time Tag");
    ZDFP[232] = std::make_tuple(10,-1, 0, "SSRZ 1hour- 5seconds Time Tag");
    ZDFP[233] = std::make_tuple(12,-1, 0, "SSRZ 1hour- 1second Time Tag");
    ZDFP[234] = std::make_tuple(17,-1, 0, "SSRZ 1day-1second Time Tag");
    // SSRZ Update Message Data fields
    ZDFP[301] = std::make_tuple( 2, 0, 0, "Scale Factor indicator");
    ZDFP[302] = std::make_tuple( 2, 0, 0, "Bin Size Indicator");
    ZDFP[303] = std::make_tuple( 1,-1, 0, "Sign Bit");
    ZDFP[304] = std::make_tuple( 1, 0, 0, "Rice Quotient");
    ZDFP[305] = std::make_tuple( 0,-1, 0, "Rice Remainder");
    //SSRZ Low Rate Correction Message Data Fields
    ZDFP[309] = std::make_tuple( 0,-1, 0, "SSRZ BE IOD");
    ZDFP[310] = std::make_tuple( 1,-1, 0, "Phase Bias Flag");
    ZDFP[311] = std::make_tuple( 0,-1, 0, "Phase Bias Indicator");
    ZDFP[312] = std::make_tuple( 1, 0, 0, "Overflow/Discontin uity Indicator");
    ZDFP[315] = std::make_tuple( 1,-1, 0, "SSRZ predictor flag");
    ZDFP[320] = std::make_tuple( 1,-1, 0, "SSRZ QIX Bias Metadata flag");
    // SSRZ Global VTEC Ionosphere Correction Data Fields
    ZDFP[330] = std::make_tuple( 1,-1, 0, "SSRZ VTEC flag");
    ZDFP[331] = std::make_tuple( 3, 1, 0, "SSRZ Global VTEC bin size indicator");
}

// Function to retrieve default parameters based on the provided value
ParameterTuple getDefaultParameters(int value) {
    auto it = ZDFP.find(value);
    if (it != ZDFP.end()) {
        return it->second;
    }
    // Return a default value or throw an exception for invalid value
    return std::make_tuple(8,-1, 0, "DUMMY"); // Get 8 bits by default
}

// Function to store the decoded value and timestamp based on the integer value
void storeSSRV(int value, int decodedValue) {
    SSRValue decoded;
    decoded.value = decodedValue;
    decoded.timestamp = std::chrono::system_clock::now();
    SSRdata[value] = decoded;
}

// Function to retrieve the decoded value and timestamp based on the integer value
SSRValue getSSRVo(int value) {
    auto it = SSRdata.find(value);
    if (it != SSRdata.end()) {
        return it->second;
    }
    // Return a default value or throw an exception for missing value
    return {0, std::chrono::system_clock::time_point()};
}

// A litte bit more comfort ...
std::tuple<int, std::chrono::system_clock::time_point, double> getSSRV(int value) {
    auto it = SSRdata.find(value);
    if (it != SSRdata.end()) {
        SSRValue& decoded = it->second;

        // Calculate the time difference
        std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
        std::chrono::duration<double> delta = currentTime - decoded.timestamp;

        return std::make_tuple(decoded.value, decoded.timestamp, delta.count());
    }

    // Return default values or throw an exception for missing value
    return std::make_tuple(0, std::chrono::system_clock::time_point(), 0.0);
}

// Function to convert a byte array to a continuous bit array
std::vector<bool> byteArr2Bits(const uint8_t data[], size_t length) {
    std::vector<bool> bitArray;
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        for (int j = 7; j >= 0; j--) {
            //fprintf(stderr,"%d",byte & (1 << j));
            bitArray.push_back(byte & (1 << j));
        }
    }
    return bitArray;
}

// Function to extract an integer value from a bit array using prefix codes
int getIoB(const std::vector<bool>& bitArray, size_t& index, int zdf, int N0 = 0, int db = -1, int Nmax = 0) {
    ParameterTuple parameters = getDefaultParameters(zdf);
    if ( N0   ==  0) N0   = std::get<0>(parameters);
    if ( db   == -1) db   = std::get<1>(parameters);
    if ( Nmax ==  0) Nmax = std::get<2>(parameters);
    std::string zdfdesc = std::get<3>(parameters);
/*    if (zdf == 2) {
    	fprintf(stderr,"\nZDF002 N0=%d db=%d Nmax=%d data=%s ",N0,db,Nmax,zdfdesc.c_str()); // std::cout << "String Value: " << stringValue << std::endl;
    } */

    int value = 0;
    int length = N0;
    int ones = 0;
    int blocks = 0;
    int lbval = 0;

    // std::cout << "PC(" << N0 << "," << db << "," << Nmax << ")= ";
    while (length > 0) {
        ones += bitArray[index];
        //std::cout << bitArray[index];
        lbval = (lbval << 1) | bitArray[index];
        // if (zdf == 2) { fprintf(stderr,"%d",bitArray[index]&1); }
        index++;
        length--;

        if (db != -1 && length == 0 && ones == (N0 + db * blocks)) {
            value += (1 << (N0 + db * blocks)) - 1;
            blocks++;
            length += ((N0 + blocks * db) > Nmax && Nmax != 0) ? Nmax : (N0 + blocks * db);
            ones = 0;
            lbval = 0;
        }
    }
    
    //std::cout << " -> res=" << value + lbval << std::endl;
    value += lbval;

    int addOneZDF[] = {2, 10, 11, 27, 55, 56, 58, 94, 95, 105, 130, 143, 144};
    if (std::find(std::begin(addOneZDF), std::end(addOneZDF), zdf) != std::end(addOneZDF)) {
    	value++;
    }
    //if (zdf == 2) { fprintf(stderr," val=%d\n",value); }

    storeSSRV(zdf, value);
    return value;
}

// Debug function with variable arguments
void debugPrintC(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

// Debug function with variable arguments using std::cout
void debugPrintCPP(const char* callerFunction, const char* format, ...) {
    va_list args;
    va_start(args, format);

    // Determine the size of the formatted string
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    // Allocate a buffer and format the string
    char* buffer = new char[size + 1];
    std::vsnprintf(buffer, size + 1, format, args);

    // Output to std::cout
    // std::cout << buffer;
    // std::cout << "[" << __FILE__ << "][" << __FUNCTION__ << "][" << __LINE__ << "]: " << buffer;
    std::cout << "[" << callerFunction << "]: " << buffer;

    // Clean up
    delete[] buffer;

    va_end(args);
}

// Wrapper macro to capture __FUNCTION__ or __func__
#define dbg(format, ...) debugPrintCPP(__func__, format, ##__VA_ARGS__)

/* NOTE SSRZ Metadata Message Processing */

/* ZDB001 Rice-encoded integer value 
   The Rice-encoded integer value (block) allows the decoding of the integer value 
   𝑚 = 𝑠(2^𝑝 𝑞 + 𝑟) */
int_fast64_t rdZDB001(const std::vector<bool>& bitArray, size_t& bitidx, size_t binsize_p) {
	uint_fast8_t  ZDF303;
	uint_fast64_t ZDF304, ZDF305;
	int_fast64_t  ival;
	//  ((N0 + blocks * db) > Nmax && Nmax != 0) ? Nmax : (N0 + blocks * db);
      	// ZDF096 = getIoB(bitArray, bitidx, 96); // Grid Bin Size Parameter
      	ZDF303 = getIoB(bitArray, bitidx, 303); // Symbol s - Sign Bit
	ZDF304 = getIoB(bitArray, bitidx, 304);            // Symbol q - Rice Quotient
	ZDF305 = getIoB(bitArray, bitidx, 305, binsize_p); // Symbol r - Rice Remainer
	ival = ((1LL<<binsize_p)*ZDF304+ZDF305)*((ZDF303==0)?1:-1); 
//	dbg("s(Z303)=%d p=%d q(Z304)=%d r(Z305)=%d ",ZDF303,binsize_p,ZDF304,ZDF305); 
	dbg("m=s*(2^p*q+r)=%d*(%llu*%llu+%llu)=%lld ",(ZDF303==0)?1:-1,1LL<<binsize_p,ZDF304,ZDF305,ival);
	return ival;
}		      	 	


// ZM011 SSRZ Satellite Group Definition Message
void procZM011(const std::vector<bool>& bitArray, size_t& bitidx) {
	int    ZDF002, ZDF012, ZDF013=0, ZDF014, ZDF015, ZDF020;
	int    nsat, j, k;
	size_t l;
	ZDF020 = getIoB(bitArray, bitidx, 20);     // SSRZ Metadata Tag
	// dbg(">>> ZM011 Z20=%d ",ZDF020);
	if (ZDF020 == 1) {
		// ZMB011 SSRZ Satellite Group Definition Metadata Message Block
		   for (j = 0; j < 2; j++) {
	   		nsat = getIoB(bitArray, bitidx, 10+j); // ZDF010/ZDF011 Number of SSRZ Low Rate Satellite Groups
	   		// dbg("\n>> ZMB011.%d Z%d=%d ",j+1,10+j,nsat);
	   		// ZDB017 SSRZ Satellite Group Definition 
	   		   ZDF012 = getIoB(bitArray, bitidx, 12); 		        // SSRZ GNSS ID Bit Mask
	   		   // dbg("\n> ZDB017.%d Z12=%d(%d) ",j+1,cntBits(ZDF012),ZDF012);
	   		   for (k = 0; k < cntBits(ZDF012); k++) {
		   	   	ZDF013 = getIoB(bitArray, bitidx, 13);  // SSRZ Maximum Satellite ID per GNSS and Group
		   		// dbg("Z13.%d=%d ",k+1,ZDF013);
			   }
		   	   ZDF014 = getIoB(bitArray, bitidx, 14);		        // Satellite Group Definition Mode
		   	   // dbg("Z14=%d ",ZDF014);
		   	   if ( ZDF014 == 0 ) {
		   	   	for (k = 0; k < cntBits(ZDF012); k++) {
		   			ZDF015 = getIoB(bitArray, bitidx, 15, ZDF013);  // Satellite Group Bit Mask per GNSS
		   			// dbg("Z15.%d=%d ",k+1,ZDF015);
              	   			

				}
			   }
		   }
		   // dbg(" <<<\n");
	}
	/*fprintf(stderr,"bi=%d bu=%d mntb=%d ",bitidx,bitidx-(bitidx-sob),mtnb*8+40);
	pmArr.push_back(bitidx); // Set End of Data Field Marker for Output*/
		
	// DEBUG: Print Rest of field 
	// for (l = bitidx; l < bitArray.size(); l++) { dbg("%d",bitArray[l]&1); } //fprintf(stderr,"");
	ZDF002 = getIoB(bitArray, bitidx, 2);     // SSRZ Metadata Tag
	// dbg("Next ZDF002 = %02d?\n", ZDF002);
	dbg("Next ZDF002 = %02d?\n", ZDF002);
}

/*		if (i == 0) {
			fprintf(stderr,"Δbits?0=%d",bitidx-si);
		} fprintf(stderr,"\n");*/


/* ZMB001 SSRZ High Rate Metadata Message Block
   STATUS: OK					*/
void procZMB001(const std::vector<bool>& bitArray, size_t& bitidx) {
	// dbg(">>> SSRZ High Rate Metadata Message Block\n"); // int bitsused=bitidx;
	int nosatgrp, nodb, nocorpar, defres,k,i,j;
	int ZDF011, ZDF012, ZDF016, ZDF043, ZDF044=0, ZDF053, ZDF054;
	for (i = 0; i < 3; i++) { // High Rate Timing, Clock, Orbit
		if ( i == 0 ) { 
			ZDF011 = getIoB(bitArray, bitidx, 11);   // Number of SSRZ HR satellite groups
			dbg("*** ZMB001 ZDF011=NG(HR)=%d ", ZDF011);
		} else {        ZDF044 = getIoB(bitArray, bitidx, 44); } // Number of SSRZ High Rate clock correction parameters
		// pmArr.push_back(bitidx); // Store marker for debugging purposes
	
		defres = (ZDF044>0&&i>0)?getIoB(bitArray, bitidx, 59+i):0; 			// ZDF060 / ZDF061
		// pmArr.push_back(bitidx); // Store marker for debugging purposes			
		nodb   = ((ZDF044>0)||(i==0))?getIoB(bitArray, bitidx, (i==0)?55:42):0;  	// ZDF055 / ZDF042
		dbg("ZDF0%3d=%d\n",((ZDF044>0)||(i==0))?((i==0)?55:42):0,nodb);
		// pmArr.push_back(bitidx); // Store marker for debugging purposes			
		if ( i == 0 ) {      dbg("**** ZMB001->ZDF055: %d Timing Blocks\n",nodb);
		} else if (i == 1) { dbg("**** ZMB001->ZDF042: %d Clock Blocks with %d cor.par.\n",nodb,ZDF044);
		} else {             dbg("**** ZMB001->ZDF042: %d Orbit Blocks with %d cor.par.\n",nodb,ZDF044); }
	
		if ( i == 0 ) { // ZDB019>(ZDB018(ZDF053>ZDF054)+ZDF016) x n=ZDF055
			for (k = 0; k < nodb; k++) {
				ZDF053   = getIoB(bitArray, bitidx, 53); // ZDF053 Length of SSR Update interval
				ZDF054   = getIoB(bitArray, bitidx, 54); // ZDF054 Offset of SSR Update interval
				ZDF016   = getIoB(bitArray, bitidx, 16, ZDF011); // ZDF016
				dbg("**** ZDF016=%d ZDF053=5d ZDF054=%d ****", ZDF016, ZDF053, ZDF054);
				UpdSTG(&SatTimGrp, ZDF016, ZDF053, ZDF054);
				//UpdColl(&SatTimGrp, ZDF016, ZDF053, ZDF054); ... not working
				OutSTG(&SatTimGrp); 
			}
		} else if (ZDF044 > 0) { // ZDB021 ... repeats NRB(clock/orbit) times
			for (k = 0; k < nodb; k++) {  	   
				ZDF012   = getIoB(bitArray, bitidx, 12);
				// dbg(">> ZDB021 ZDF012= [...ICSJERG] = ",ZDF012); // See Table 10.1
				
				std::bitset<16> bits(ZDF012); std::cout << bits; // << std::endl;
				for (j = 0; j < ZDF044; j++) { 
					ZDF043   = getIoB(bitArray, bitidx, 43); // ZDF043 Default Bin Size Parameter
					std::cout << " dx" << j << "=" << ZDF044;
					UpdHRCB(&HRClk, ZDF012, ZDF044, ZDF043);
				} std::cout << std::endl; 
				OutHRCB(&HRClk); 
			}
		}
	} 
        // dbg("bi=%ld ",bitidx);
        //pmArr.push_back(bitidx); // Store marker for debugging purposes
}

/* ZMB002 SSRZ Low Rate Metadata Message Block
   STATUS: Huge amount of unknown bits at the end	*/
void procZMB002(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
      // dbg(">>> ZMB002 SSRZ Low Rate Metadata Message Block [%d]\n",ZDF020);
      int ZDF011, ZDF055, nodb, defres, ZDF065, ZDF066, ZDF010, ZDF067, ZDF068, ZDF069, ZDF070;
      int ZDF012, ZDF199, ZDF200, ZDF201, ZDF202, ZDF203, ZDF204, ZDF205, ZDF206, ZDF207, ZDF208, ZDF209, ZDF210, ZDF211, ZDF212, ZDF213, ZDF214, ZDF215, ZDF216;
      int ZDF019, ZDF046, ZDF071, ZDF072, ZDF073, ZDF134, ZDF110, ZDF111, ZDF112, ZDF114, ZDF113, ZDF115, ZDF074, ZDF116;
      int ZDF062, ZDF063, ZDF064, ZDF042, ZDF043, ZDF045, ZDF044, ZDF053, ZDF054, ZDF016;
      int i,j,k;
      
      ZDF010 = getIoB(bitArray, bitidx, 10); // ZDF010 Number of LR satellite groups
      for (i = 0; i < 6; i++) { /* i=0 main header
				   i=1 Low Rate Clock Correction Metadata
				   i=2 Low Rate Orbit Correction Metadata
				   i=3 Low Rate Velocity Corrections Metadata
				   i=4 Low Rate Code Bias Metadata
				   i=5 Low Rate Phase Bias Metadata
				   i=6 Low Rate Satellite-dependet Global Ionosphere Correction Metadata */
	    if ( i == 0 ) { // ZDB019>(ZDB018(ZDF053>ZDF054)+ZDF016) x n=ZDF055
		  if (ZDF020 == 1) {
			nodb = getIoB(bitArray, bitidx, 55); // ZDF055 Number of Satellite-dependent Timing Blocks
			  for (k = 0; k < nodb; k++) { 
				// ZDB019
				   // ZDB018
				      ZDF053   = getIoB(bitArray, bitidx, 53);         // Length of SSR Update interval
				      ZDF054   = getIoB(bitArray, bitidx, 54);         // Offset of SSR Update interval
				   ZDF016   = getIoB(bitArray, bitidx, 16, ZDF010); // SSRZ Satellite Group List Bit Mask
				// dbg(">> NGLR(Z10)=%d NTIM(Z55)=%d ZDF053=%d ZDF054=%d ZDF016=%d ",ZDF010,nodb,ZDF053,ZDF054,ZDF016); // See Table 10.1
			  }
			  // ZDB030 SSRZ BE IOD Definition Block
			     ZDF012 = getIoB(bitArray, bitidx, 12);  // SSRZ GNSS ID Bit Mask
			     // dbg(" Z12=%d(%d) ",  cntBits(ZDF012), ZDF012);
			     for (k = 0; k < cntBits(ZDF012); k++) {
				     ZDF199 = getIoB(bitArray, bitidx, 199); // Length of the SSRZ BE IOD field
				     // dbg("Z199.%d=%d ",k+1,ZDF199);
			     }       
			     ZDF200 = getIoB(bitArray, bitidx, 200); // SSRZ BE IOD Tag 1->use IODs without modification
			  // dbg(" Z200=%d ", ZDF200);
		  }
		  if (ZDF020 == 2) {
			  ZDF201 = getIoB(bitArray, bitidx, 201);
			  ZDF202 = getIoB(bitArray, bitidx, 202);
			  ZDF203 = getIoB(bitArray, bitidx, 203);
			  ZDF204 = getIoB(bitArray, bitidx, 204);
			  ZDF205 = getIoB(bitArray, bitidx, 205);
			  ZDF206 = getIoB(bitArray, bitidx, 206);
			  ZDF207 = getIoB(bitArray, bitidx, 207);
			  ZDF208 = getIoB(bitArray, bitidx, 208);
			  ZDF209 = getIoB(bitArray, bitidx, 209);
			  ZDF210 = getIoB(bitArray, bitidx, 210);
			  ZDF211 = getIoB(bitArray, bitidx, 211);
			  ZDF212 = getIoB(bitArray, bitidx, 212);
			  ZDF213 = getIoB(bitArray, bitidx, 213);
			  ZDF214 = getIoB(bitArray, bitidx, 214);
			  ZDF215 = getIoB(bitArray, bitidx, 215);
			  ZDF216 = getIoB(bitArray, bitidx, 216);
		 }
	    }
	    if (i >= 1 && i <= 3) {
		 //if (i == 1) // dbg("\n>> ZMB002 SSRZ Low Rate Clock Correction Metadata\n");
		 //if (i == 2) // dbg("\n>> ZMB002 SSRZ Low Rate Orbit Corrections Metadata\n");
		 //if (i == 3) // dbg("\n>> ZMB002 SSRZ Low Rate Velocity Corrections Metadata\n");
		 ZDF044 = 0;
		 if (ZDF020 == 1) { ZDF044 = getIoB(bitArray, bitidx, 44); } // Number of elements
		 // dbg("Z44=%d ",ZDF044);
		 // Divers
		 if ((i == 1) && (ZDF020 == 1)) {        // Low Rate Clock
			ZDF062 = getIoB(bitArray, bitidx, 62);
			// dbg("Z62=%d ",ZDF062);
			if (ZDF044 == 2) {
				ZDF063 = getIoB(bitArray, bitidx, 63); 
				// dbg("Z63=%d ",ZDF063);
			}
		 } else if ((i == 2) && (ZDF020 == 1)) { // Low Rate Orbit
			ZDF064 = getIoB(bitArray, bitidx, 64); 
			ZDF065 = getIoB(bitArray, bitidx, 65);
			ZDF066 = getIoB(bitArray, bitidx, 66);
			// dbg("Z64=%d Z65=%d Z66=%d ",ZDF064,ZDF065,ZDF066);
		 } else if ((i == 3) && (ZDF020 == 1)) { // Low Rate Velocity
			if (ZDF044 == 3) {
				ZDF067 = getIoB(bitArray, bitidx, 67); 
				ZDF068 = getIoB(bitArray, bitidx, 68);
				ZDF069 = getIoB(bitArray, bitidx, 69);
				// dbg("Z67=%d Z68=%d Z69=%d ",ZDF067,ZDF068,ZDF069);
			}
		 }
		 if ((ZDF020 == 1) && (ZDF044 > 0)) {
			ZDF042 = getIoB(bitArray, bitidx, 42);    // Number of Rice Blocks
			// dbg("Z42=%d ",ZDF042);
			for (k = 0; k < ZDF042; k++) {
				// ZDB021
				   ZDF012 = getIoB(bitArray, bitidx, 12); // GNSS ID Bit Mask
				   ZDF043 = getIoB(bitArray, bitidx, 43); // Default Bin Size Parameter
				   // dbg("ZDB021.%d Z12=%d Z43=%d ",k+1,ZDF012,ZDF043);
			}
		 }
	    } 
	    //if (i == 4) // dbg("\n>> ZMB002 Low Rate Code Bias Metadata\n");
	    //if (i == 5) // dbg("\n>> ZMB002 Low Rate Phase Bias Metadata\n");
	    //if (i == 6) // dbg("\n>> ZMB002 Low Rate Satellite-dependent Global Ionosphere Correction Metadata\n");
	    if ((i == 4) && (ZDF020 == 1)) {        // SSRZ Low Rate Code Bias Metadata
		ZDF070 = getIoB(bitArray, bitidx,  70); // SSRZ Code Bias Default Resolution
		ZDF045 = getIoB(bitArray, bitidx,  45); // SSRZ Number of Code Bias Rice
		// dbg("Z70=%d Z45=%d ",ZDF070,ZDF045);
		for (j = 0; j < ZDF045; j++) {
			// ZDB022 SSRZ Code Bias Signal Rice Block Definition
			   // ZDB020 SSRZ Signal Bit Mask Block
			      ZDF012 = getIoB(bitArray, bitidx, 12); // SSRZ GNSS ID Bit Mask
			      // dbg("ZDB022.%d Z12=%d(%d) ",j+1,cntBits(ZDF012),ZDF012);
			      for (k = 0; k < cntBits(ZDF012); k++) {
				      ZDF019 = getIoB(bitArray, bitidx, 19); // SSRZ Signal Bit Mask per GPS
				      // dbg("Z19.%d=%d ",k+1,ZDF019);
			      }
			   ZDF046 = getIoB(bitArray, bitidx, 46);    // Default Bin Size Parameter
			   // dbg("Z46=%d ",ZDF046);
		}
	    }
	    if ((i == 4 || i == 5) && (ZDF020 == 1)) {
		// ZDB020 SSRZ Code Bias Reference Signal Bit Mask Block
		   ZDF012 = getIoB(bitArray, bitidx, 12); // SSRZ GNSS ID Bit Mask
		   // dbg("ZDB020 Z12=%d(%d) ",cntBits(ZDF012),ZDF012);
		   for (k = 0; k < cntBits(ZDF012); k++) {
			   ZDF019 = getIoB(bitArray, bitidx, 19); // SSRZ Signal Bit Mask per GPS
			   // dbg("Z19.%d=%d ",k+1,ZDF019);
		   }
	    }
	    if ((i == 5) && (ZDF020 == 1)) { // SSRZ Low Rate Phase Bias Metadata
		ZDF071 = getIoB(bitArray, bitidx,  71); // SSRZ Low Rate Phase Bias Cycle Range
		ZDF072 = getIoB(bitArray, bitidx,  72); // SSRZ Low Rate Phase Bias Bitfield Length
		ZDF073 = getIoB(bitArray, bitidx,  73); // SSRZ Maximum Number of Continuity/ Overflow bits
		// dbg("Z71=%d Z72=%d Z73=%d ",ZDF071,ZDF072,ZDF073);
	    } else if ((i == 6) && (ZDF020 == 1)) { // SSRZ Low Rate Satellite-dependent Global Ionosphere Correction Metadata
		ZDF134 = getIoB(bitArray, bitidx, 134); // Satellite dependent Global Ionosphere Correction Flag
		// dbg("Z134=%d ",ZDF134);
		if (ZDF134 == 1) {
			// ZDB060 SSRZ Model Parameters Block
			   ZDF110 = getIoB(bitArray, bitidx, 110);      // Model ID
			   ZDF111 = getIoB(bitArray, bitidx, 111);      // Model Version
			   ZDF112 = getIoB(bitArray, bitidx, 112);      // NInt
			   // dbg("Z110=%d Z111=%d Z112=%d ",ZDF110,ZDF111,ZDF112);
			   for (j = 0; j < ZDF112; j++) {
				ZDF114 = getIoB(bitArray, bitidx, 114); // 
			   // dbg("ZDF112.%d=%d ",j+1, ZDF114);
			   }
			   ZDF113 = getIoB(bitArray, bitidx, 113);      // Nfloat
			   // dbg("Z113=%d ",ZDF113);
			   for (j = 0; j < ZDF113; j++) {
				ZDF115 = getIoB(bitArray, bitidx, 115); //
				// dbg("ZDF113.%d=%d ",j+1, ZDF115);
			   }
			 ZDF074 = getIoB(bitArray, bitidx,  74); // 
			 // dbg("Z74=%d ",ZDF074);
			 // ZDB023 SSRZ Compressed Coefficient Data Block Definition
			    ZDF042 = getIoB(bitArray, bitidx, 42); // Number of Rice Blocks NRB
			    // dbg("ZDB023 Z42=%d ",ZDF042);
			    for (j = 0; j < ZDF042; j++) {
				// ZDB024 SSRZ Coefficient Rice Block Definition
				   ZDF116 = getIoB(bitArray, bitidx, 116); // Coefficient Order Bit Mask
				   ZDF043 = getIoB(bitArray, bitidx,  43); // Default Bin Size Parameter
				   // dbg("ZDB024.%d Z116=%d Z43=%d ",j+1,ZDF116,ZDF043);
			    }
		}
	    }
      }
      // dbg("\n>> ZMB002 UNKNOWN: "); for (j = 0; j < 197 ; j++) {
	// dbg("%d",getIoB(bitArray, bitidx, 0, 1,-1,0));
      //} // dbg(" <<\n");
}


/* ZMB003 SSRZ Gridded Ionosphere Correction Metadata Message Block
   STATUS: OK								*/
void procZMB003(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	// dbg("\n>> ZMB003-%d: ",ZDF020);
	int ZDF010, ZDF055,ZDF056, ZDF057, ZDF058, ZDF026, ZDF075, ZDF025, j, ZDF043, ZDF053, ZDF054, ZDF016;
	if (ZDF020 == 1) {       // ZMB003-1 SSRZ Gridded Ionosphere Correction Metadata Message Block for tag 1
		ZDF010 = getIoB(bitArray, bitidx, 10);  // ZDF010 Number of SSZR LR satellite groups
		ZDF055 = getIoB(bitArray, bitidx, 55);  // ZDF055 Number of Satellite-dependet Timing Blocks
		// dbg( "ZDF010->NG(LR)=%d ZDF055->NTiming=%d ZDB019[TB]=%d ",ZDF010,ZDF055,ZDF010*(ZDF055+12));
		for (j = 0; j < ZDF055; j++) { 	// ZDB019 SSRZ GRI Timing Block
			ZDF053 = getIoB(bitArray, bitidx, 53);          // ZDF053 Length of SSR Update Interval
			ZDF054 = getIoB(bitArray, bitidx, 54);          // ZDF054 Offset of SSR Update Interval
			ZDF016 = getIoB(bitArray, bitidx, 16, ZDF010);  // ZDF016 Satellite Group List Bit Mask
			// // dbg("\n#%d: ZDF053=%d ZDF054=%d ZDF016=",j,ZDF053,ZDF054); std::bitset<16> bits(ZDF016); std::cout << bits << std::endl;
		} 
	} else if (ZDF020 == 2) { // ZDB044 Grid-related Timing Parameters
		ZDF025 = getIoB(bitArray, bitidx, 25);  // ZDF025 Maximum Grid ID
		ZDF058 = getIoB(bitArray, bitidx, 58);  // ZDF058 Number of SSRZ Grid-related Update and Offset Block
		for (j = 0; j < ZDF058; j++) { 	// ZDB043 SSRZ Grid-related Update and Offset Block
			// ZDB040
			ZDF056 = getIoB(bitArray, bitidx, 56);  // ZDF056 SSR Length of SSR Update Interval
			ZDF057 = getIoB(bitArray, bitidx, 57);  // ZDF057 SSR Update Interval Offset
			ZDF026 = getIoB(bitArray, bitidx, 26, ZDF025);  // ZDF026 SSRZ Grid ID Bit Mask
		}
	}	
	ZDF075 = getIoB(bitArray, bitidx, 75);  // ZDF075 Default Resolution of the Gridded Ionospehre Corrections
	ZDF043 = getIoB(bitArray, bitidx, 43);  // ZDF043 Default Bin Size Parameter
	// dbg(" dx0=%d p0=%d\n", ZDF075, ZDF043);
}

/* ZMB004 SSRZ Gridded Troposphere Correction Metadata Message Block
   STATUS: Verified based on Appendix, but 10 unknown bits at tail */
void procZMB004(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	int ZDF025, ZDF026, ZDF056, ZDF057, ZDF058, ZDF076, ZDF107, ZDF110, ZDF111, ZDF112, ZDF113, ZDF114, ZDF115;
	int ZDF053, ZDF054,j, ZDF043;
	// dbg("\n>> ZMB004-%d: ",ZDF020);
	if (ZDF020 == 2) {	  // ZMB004-2
		// ZDB018 SSRZ Timing Block
		   ZDF053 = getIoB(bitArray, bitidx, 53); // Length of SSR Update Interval
		   ZDF054 = getIoB(bitArray, bitidx, 54); // Offset of ...
		   // dbg("Z53=%d Z54=%d ", ZDF053, ZDF054);
	} else if (ZDF020 == 3) { // ZMB004-3
		// ZDB044 SSRZ Grid-related Timing Parameters
		   ZDF025 = getIoB(bitArray, bitidx, 25); // MGridID
		   ZDF058 = getIoB(bitArray, bitidx, 58); // NTmgGrd
		   // dbg("Z25=%d Z58=%d ", ZDF025, ZDF058);
		   for (j = 0; j < ZDF058; j++) {
			// ZDB043 Grid related Update and Offset Block
			   // ZDB040
			      ZDF056 = getIoB(bitArray, bitidx, 56); // Update
			      ZDF057 = getIoB(bitArray, bitidx, 57); // Offset
			   ZDF026 = getIoB(bitArray, bitidx, 26, ZDF025); // Grid ID Bit Mask
		   }
	}
	// ZDB060 SSRZ Model Parameter Block
	   ZDF110 = getIoB(bitArray, bitidx, 110);      // Model ID
	   ZDF111 = getIoB(bitArray, bitidx, 111);      // Model Version
	   ZDF112 = getIoB(bitArray, bitidx, 112);      // NInt
	   // dbg("Z110=%d Z111=%d Z112=%d ",ZDF110,ZDF111,ZDF112);
	   for (j = 0; j < ZDF112; j++) {
		ZDF114 = getIoB(bitArray, bitidx, 114); // 
		// dbg("ZDF112.%d=%d ",j+1, ZDF114);
	   }
	   ZDF113 = getIoB(bitArray, bitidx, 113);      // Nfloat
	   // dbg("Z113=%d ",ZDF113);
	   for (j = 0; j < ZDF113; j++) {
		ZDF115 = getIoB(bitArray, bitidx, 115); //
		// dbg("ZDF113.%d=%d ",j+1, ZDF115);
	   }
	ZDF107 = getIoB(bitArray, bitidx, 107); // Tropo Comp bit mask
	// dbg("ZDF107=%d ",ZDF107);
	for (j = 0; j < 3; j++) {              // Iterate for Bits 0,1,2
		if ((ZDF107 & (1 << j)) != 0) {
			ZDF076 = getIoB(bitArray, bitidx, 76); // Scale Factor
			ZDF043 = getIoB(bitArray, bitidx, 43); // Default Bin
			// dbg("Z107.%d Z76=%d Z43=%d ", j+1, ZDF076, ZDF043);
		}
	}
	// dbg("UNKNOWN: "); for (j = 0; j < 10 ; j++) {
		// dbg("%d",getIoB(bitArray, bitidx, 0, 1,-1,0));
	//} // dbg(" <<\n");
}

/* ZMB005 SSRZ Satellite dependent Regional Ionosphere Correction Metadata Message Block
   Status: 99%IO, ZDF116 unklar, warum 1 Bit mehr erforderlich 			*/
void procZMB005(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	// dbg("SSRZ Satellite dependent Regional Ionosphere Correction Metadata Message Block\n");
	int ZDF010, ZDF080, ZDF110, ZDF111, ZDF112, ZDF113, ZDF114, ZDF115, ZDF116, ZDF147, ZDF148, ZDF149, ZDF150;
	int ZDF055, j, ZDF053, ZDF054, ZDF016, ZDF042, ZDF043;
	int bitback;
	ZDF010 = getIoB(bitArray, bitidx, 10); // Number of SSRZ LR sat groups
	ZDF055 = getIoB(bitArray, bitidx, 55); // Number of satellite-dependent Timing Blocks
	// dbg(">>Z10=%d Z55=%d ",ZDF010,ZDF055);
	for (j = 0; j < ZDF055; j++) {
		// ZDB019 Timing Block of satellitedependent ionosphere corrections
		   // ZDB018
		      ZDF053 = getIoB(bitArray, bitidx, 53); // Length of Update Int
		      ZDF054 = getIoB(bitArray, bitidx, 54); // Offset
		   ZDF016 = getIoB(bitArray, bitidx, 16, ZDF010);    // SSRZ Sat Group List bit
		   // dbg(">ZDB019.%d Z53=%d Z54=%d Z16=%d ", j+1,ZDF053, ZDF054, ZDF016);
	}
	// ZDB060
	   ZDF110 = getIoB(bitArray, bitidx, 110);      // ZDF110 Model ID
	   ZDF111 = getIoB(bitArray, bitidx, 111);      // ZDF111 Model Version
	   ZDF112 = getIoB(bitArray, bitidx, 112);      // ZDF112 Number of Integer Model Parameters
	   // dbg("Z110=%d Z111=%d Z112=%d ", ZDF110, ZDF111, ZDF112);
	   for (j = 0; j < ZDF112; j++) { 	
		ZDF114 = getIoB(bitArray, bitidx, 114); // ZDF114
		// dbg("ZDF114.%d=%d ",j+1, ZDF114);
	   }
	   ZDF113 = getIoB(bitArray, bitidx, 113);      // ZDF113 Nfloat
	   // dbg("Z113=%d ",ZDF113);
	   
	   for (j = 0; j < ZDF113; j++) { 	
		ZDF115 = getIoB(bitArray, bitidx, 115); // ZDF115 Float Model Parameters
		// dbg("ZDF115.%d=%.1f ",j+1,i2f(ZDF115)); 
	   }
	ZDF080 = getIoB(bitArray, bitidx, 80); // Default Resolution
	// ZDB023
	   ZDF042 = getIoB(bitArray, bitidx, 42); // NRB
	   // dbg("Z80=%d Z42=%d ",ZDF080,ZDF042);
	   for (j = 0; j < ZDF042; j++) {
		// ZDB024 Coeff Rice Block Def
		   ZDF116 = getIoB(bitArray, bitidx, 116, ZDF010+1); // Coeff Order Bit Mask +1 Update ggü. Manual
		   ZDF043 = getIoB(bitArray, bitidx, 43);  // Default Bin Size
		   // dbg("ZDB024.%d: Z116=%d Z43=%d ",j+1,ZDF116,ZDF043); 
	   }
	// dbg("<<\n");
}

/* ZMB006 SSRZ Global VTEC Ionosphere Correction Metadata Message Block
   STATUS: Running Smoothley						*/
void procZMB006(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	// dbg("SSRZ Global VTEC Ionosphere Correction Metadata Message Block [#%d]\n",ZDF020);
	int ZDF110, ZDF111, ZDF112, ZDF113, ZDF114, ZDF115, ZDF116;
	int ZDF056, ZDF057, ZDF081, ZDF082, ZDF129, ZDF130, ZDF131, ZDF132, ZDF133;
	int j, ZDF042, ZDF043, ZDF053, ZDF054;
	if (ZDF020 == 2 || ZDF020 == 3) {
		// ZDB018 SSRZ Timing Block
		   ZDF053 = getIoB(bitArray, bitidx, 53); // ZDF053 Length of SSR Update interval
		   ZDF054 = getIoB(bitArray, bitidx, 54); // ZDF054 Offset of SSR Update interval
		   // dbg("Z53=%d Z54=%d ",ZDF053,ZDF054);
	} else if (ZDF020 == 4) {
		// ZDB040
		   ZDF056 = getIoB(bitArray, bitidx, 56);  // ZDF056 SSR Length of SSR Update Interval
		   ZDF057 = getIoB(bitArray, bitidx, 57);  // ZDF057 SSR Update Interval Offset
	}
	if (ZDF020 == 2 || ZDF020 == 3) {
		// ZDB060
		   ZDF110 = getIoB(bitArray, bitidx, 110);      // ZDF110 Model ID
		   ZDF111 = getIoB(bitArray, bitidx, 111);      // ZDF111 Model Version
		   ZDF112 = getIoB(bitArray, bitidx, 112);      // ZDF112 Number of Integer Model Parameters
		   // dbg("Z110=%d Z111=%d Z112=%d ", ZDF110, ZDF111, ZDF112);
		   for (j = 0; j < ZDF112; j++) { 	
			ZDF114 = getIoB(bitArray, bitidx, 114); // ZDF114
			// dbg("ZDF114.%d=%d ",j+1, ZDF114);
		   }
		   ZDF113 = getIoB(bitArray, bitidx, 113);      // ZDF113 Nfloat
		   // dbg("Z113=%d ",ZDF113);
	   
		   for (j = 0; j < ZDF113; j++) { 	
			ZDF115 = getIoB(bitArray, bitidx, 115); // ZDF115 Float Model Parameters
			// dbg("ZDF115.%d=%.1f ",j+1,i2f(ZDF115)); 
		   }
		ZDF129 = getIoB(bitArray, bitidx, 129); // SSRZ Encoder Type
		if (ZDF020 == 2) { ZDF081 = getIoB(bitArray, bitidx, 81); } // Global VTEC Resolution
	}
	if (ZDF020 == 3 || ZDF020 == 4) {
		ZDF082 = getIoB(bitArray, bitidx, 82);  // Global VTEC Resolution
		if (ZDF020 == 4) {
			ZDF130 = getIoB(bitArray, bitidx, 130); // Number of Ionospheric Layers
			// ZDB061
			   ZDF131 = getIoB(bitArray, bitidx, 131); // Height of Ionospheric Layer
			   ZDF132 = getIoB(bitArray, bitidx, 132); // Spherical Harmonics Degree
			   ZDF133 = getIoB(bitArray, bitidx, 133); // Spherical Harmonics Order
			// ZDB023
			   ZDF042 = getIoB(bitArray, bitidx, 42);  // Number of Rice Blocks NRB 
			   for (j = 0; j < ZDF042; j++) {
			   // ZDB024
			      ZDF116 = getIoB(bitArray, bitidx, 116); // Coefficient Order Bit Mask
			      ZDF043 = getIoB(bitArray, bitidx, 116); // Default Bin Size
			   }
		}
	}
	// dbg("<<\n");
}

/* ZMB007 SSRZ Regional Troposphere Correction Metadata Message Block 
   STATUS: 42 Unknown bits at end					*/
void procZMB007(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	int j, k, ZDF107, ZDF110, ZDF111, ZDF112, ZDF113, ZDF114, ZDF115, ZDF085, ZDF116, ZDF108, ZDF086, ZDF027, ZDF046, ZDF030;
	int ZDF056, ZDF057, ZDF058, ZDF166, ZDF167, ZDF168, ZDF169, ZDF170, ZDF171, ZDF172, ZDF173, ZDF047, ZDF028, ZDF053, ZDF054;
	int ZDF042, ZDF043;
	
	// dbg("\n>> ZMB007-%d: ",ZDF020);
	if (ZDF020 == 2) { 	 // ZMB007-2
		// ZDB018 SSRZ Timing Block
		   ZDF053 = getIoB(bitArray, bitidx, 53);       // ZDF053 Length of SSR Update Interval
		   ZDF054 = getIoB(bitArray, bitidx, 54);       // ZDF054 Offset of SSR Update Interval
		ZDF107 = getIoB(bitArray, bitidx, 107); 	// ZDF107 Troposheric Basic Component Bit Mask
		// ZDB060 SSRZ Model Parameters Block
		   ZDF110 = getIoB(bitArray, bitidx, 110);      // ZDF110 Model ID
		   ZDF111 = getIoB(bitArray, bitidx, 111);      // ZDF111 Model Version
		   ZDF112 = getIoB(bitArray, bitidx, 112);      // ZDF112 Number of Integer Model Parameters
		   for (j = 0; j < ZDF112; j++) { 	
			ZDF113 = getIoB(bitArray, bitidx, 113); // ZDF113 Integer Model Parameter
		   }
		   ZDF114 = getIoB(bitArray, bitidx, 114);      // ZDF114 Number of Float Model Parameters
		   for (j = 0; j < ZDF114; j++) { 	
			ZDF115 = getIoB(bitArray, bitidx, 115); // ZDF115 Float Model Parameters
		   }
		for (j = 0; j < 2; j++) { // Iterate for Bits 0,1,2 of ZDF107
			if ((ZDF107 & (1 << j)) != 0) {			// ZDF085 SSRZ default resolution of the regional troposphere wet component
				ZDF085 = getIoB(bitArray, bitidx, 85);  // 
				// ZDB023 SSRZ Compressed Coefficients Block Definition of regional troposphere total component
				   ZDF042 = getIoB(bitArray, bitidx, 42);  // Number of Rice Blocks
				   // ZDB024 Coefficient Rice Block Definition
				   for (k = 0; k < ZDF042; k++) {
					ZDF116 = getIoB(bitArray, bitidx, 116);  // Coefficient Order Mask
					ZDF043 = getIoB(bitArray, bitidx, 43);   // Default Bin Size
				   }
				ZDF108 = getIoB(bitArray, bitidx, 108); // SSRZ Separated Compressed Coefficient Blocks per Height Order Flag for regional troposphere wet component
			}
		}
		if ((ZDF107 & (1 << 3)) != 0) {			
			ZDF086 = getIoB(bitArray, bitidx, 86);  // SSRZ default resolution of the regional troposphere mapping improvements 
			ZDF027 = getIoB(bitArray, bitidx, 27);  // SSRZ Compressed Parameter Data Block Definition
		}
	} else if (ZDF020 == 3) { 
		// >ZDB046 SSRZ Region-related Timing Parameters
		   ZDF027 = getIoB(bitArray, bitidx, 27);  // Maximum SSRZ Region ID
		   ZDF058 = getIoB(bitArray, bitidx, 58);  // NTmgReg
		   // dbg(" MRegId=%d NTmgReg=%d", ZDF027, ZDF058);
		   for (j = 0; j < ZDF058; j++) {
			// ZDB045   SSRZ Region-related Update and Offset Block
			   // ZDB040 SSRZ Update and Offset Block
			      ZDF056 = getIoB(bitArray, bitidx, 56); // ZDF056 Length of SSR Update Interval
			      ZDF057 = getIoB(bitArray, bitidx, 57); // ZDF057 Offset of SSR Update Interval
			   //ZDF027 = getIoB(bitArray, bitidx, 27, MRegID );    // FALSCH IM MANUAL!
			   // // dbg(">>> GET ZDF028 with %d bits bi=%d..",MRegID,bitidx);
			   //if ( j < NTmgReg) { 
			   ZDF028 = getIoB(bitArray, bitidx, 28, ZDF027 ); // SSRZ Region ID Bit Mask
			   //}   
			   // // dbg("%d<<<",bitidx);
			// dbg("\n>ZDB045.%d Update Intervall(Z56)=%2ds Offset(Z57)=%ds Region(Z28):%d<%d(Z27)",j+1,ZDF056,ZDF057,ZDF028,ZDF027+1);
		   }
		   
		// ZMB007-3 (until ZDF030 = 0)
		   // dbg("\n>> ZMB007-3 <<");
		   ZDF030 = 1;
		   do { // printf("%d\n", i); i++;
			ZDF027 = getIoB(bitArray, bitidx,  27); // ZDF027 SSRZ Regional Troposphere Region ID
			ZDF168 = getIoB(bitArray, bitidx, 168); // ZDF168 Latitude of ground point origin
			ZDF169 = getIoB(bitArray, bitidx, 169); // ZDF169 Longitude of ground point origin
			ZDF170 = getIoB(bitArray, bitidx, 170); // ZDF170 Height of ground point origin
			ZDF172 = getIoB(bitArray, bitidx, 172); // ZDF172 Horizontal Scale Factor
			ZDF173 = getIoB(bitArray, bitidx, 173); // ZDF173 Vertical Scale Factor
			ZDF107 = getIoB(bitArray, bitidx, 107); // ZDF107 Troposheric Basic Componet Bit Mask
			// dbg("\nZ30=%d RegId=%d Lat=%3.1f° Lon=%3.1f° H=%dm DRT=%d HRT=%d CBM=%d (%d)",ZDF030,ZDF027+1,(float)ZDF168*0.1,(float)ZDF169*0.1,ZDF170,100+ZDF172*10,100+ZDF173*10,ZDF107&0xf,ZDF107);

			for (j = 0; j < 3; j++) {              // Iterate for Bits 0,1,2 of ZDF107
			      // // dbg("<!><!>ZDF107=%d,l=%d,e=%d\n",ZDF107,l,(ZDF107 & (1 << j)));
			      if ((ZDF107 & (1 << j)) != 0) {						
				// dbg("\n<<< CHK: BIT=%d = %d OK <<<",j, 1<<j);            
				// ZDB050 SSRZ Basic Troposphere Component Coefficient Data Block Definition
				// ZDB051 SSRZ Tropospheric Mapping Function Improvement Parameter Data Block Definition
				   ZDF166 = getIoB(bitArray, bitidx, 166);        // ZDF166 Max Horizontal Order of reg tropo comp c
				   ZDF167 = getIoB(bitArray, bitidx, 167);        // ZDF167 Max Vertical Order of reg tropo comp c
				   // dbg("\nZ107 BIT%d Z166=%d Z167=%d ",j,ZDF166, ZDF167);
				   if (j < 3) {
					   ZDF085 = getIoB(bitArray, bitidx, 85);         // ZDF085 Resolution of reg tropo coeff of comp c
					   // dbg("Z085=%d ", ZDF085);
					   // ZDB023 SSRZ Compressed Coefficient Data Block Definition
					      ZDF042 =  getIoB(bitArray, bitidx, 42);     // NRB
					      // dbg("ZDF042=NRB=%d ", ZDF042);
					      for (k = 0; k < ZDF042; k++) {
						      // ZDB024 SSRZ Coefficient Rice Block Definition
							 ZDF116 =  getIoB(bitArray, bitidx, 116);    // Coefficient Order Bit Mask
							 ZDF043 =  getIoB(bitArray, bitidx, 43);     // Default Bin Size Parameter
							 // dbg( "ZDB024.%d ZDF116=%d ZDF043=%d ", k+1, ZDF116, ZDF043);
					      }
					   if (j == 0) {
						ZDF108 = getIoB(bitArray, bitidx, 108);   // ZDF108 Separated Compresses Coefficient Block per Height
						// dbg("Z108=%d ", ZDF108);

					   }
				   } else if ( j == 3 ) { // ZDB051 extras
					ZDF171 = getIoB(bitArray, bitidx, 171);          // ZDF171 Maximum Elevation for Mapping Improvement
					ZDF086 = getIoB(bitArray, bitidx, 86);           // ZDF086 Resolution of mapping function improvement parameters
					// ZDB027 SSRZ Compressed Parameter Data Block Definition
					   ZDF042 = getIoB(bitArray, bitidx, 42);         // NRB
					   for (k = 0; k < ZDF042; k++) {
						// ZDB028 SSRZ Parameter Rice Block Definition
						   ZDF047 =  getIoB(bitArray, bitidx, 47);     // Parameter List Bit Mask
						   ZDF043 =  getIoB(bitArray, bitidx, 43);     // Default Bin Size Parameter
					   }
				   }
			      }
			}
			ZDF030 = getIoB(bitArray, bitidx, 30);  // ZMB007-3
			// dbg(">> ZDF30=%d <<",ZDF030);
		   } while (ZDF030 > 0);
	}
	// dbg("\n>> ZMB007 UNKNOWN: "); for (j = 0; j < 42 ; j++) {
		// dbg("%d",getIoB(bitArray, bitidx, 0, 1,-1,0));
	//} // dbg(" <<\n");
}

/* ZMB008 SSRZ QIX Bias Metadata
   STATUS: UNTESTED!							*/
void procZMB008(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	int ZDF190, ZDF191, ZDF089, ZDF090, ZDF045, ZDF046, ZDF012, ZDF019, j,k;
	if (ZDF020 == 2) {
		ZDF190 = getIoB(bitArray, bitidx, 190);  // SSRZ QIX Code Bias flag
		ZDF191 = getIoB(bitArray, bitidx, 191);  // SSRZ QIX Phase Bias flag
	}
	for (j = 0; j < 2; j++) {
		if (j == 0) {
			ZDF089 = getIoB(bitArray, bitidx, 89);  // SSRZ QIX Code Bias Default Resolution
		} else if (j == 1) {
			ZDF090 = getIoB(bitArray, bitidx, 90);  // SSRZ QIX Code Bias Default Resolution
		}
		if ((ZDF020 == 1)) { // || ((ZDF020 == 2) && ((j == 0) && (ZDF190 == 1)) || ((j == 1) && (ZDF191 == 1)))) {
			ZDF045 = getIoB(bitArray, bitidx, 45);   	// SSRZ Number of QIX Code/Phase Bias Rice Blocks
			// ZDB022
			   // ZDB020 SSRZ Signal Rice Block Definition 
			   ZDF012 = getIoB(bitArray, bitidx, 12);   	// SSRZ GNSS ID Bit Mask
			   for (k = 0; k < cntBits(ZDF012); k++) {
			   	ZDF019 = getIoB(bitArray, bitidx, 19);   	// SSRZ Signal Bit Mask per GNSS
			   }
			   ZDF046 = getIoB(bitArray, bitidx, 46);   	// Default Bin Size Parameter of encoded Signal Biases
		}
	}
}

/* ZMB011 SSRZ Satellite Group Definition Metadata Message Block
   STATUS: UNTESTED!							*/
void procZMB011(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	int j,k,i,ZDF010,ZDF011,ZDF012,ZDF013,ZDF014,ZDF015;
	std::vector<int> SysArr;
	for (j = 0; j < 2; j++ ) {
		if ( j == 0 ) { 
			ZDF010 = getIoB(bitArray, bitidx, 10); // Number of SSRZ Low Rate Satellite Groups
		} else if ( j == 1 ) {
			ZDF011 = getIoB(bitArray, bitidx, 11); // Number of SSRZ High Rate Satellite Groups
		}
		for (k = 0; k < ((j==0)?ZDF010:ZDF011); k++) {
			// ZDB017
			   ZDF012 = getIoB(bitArray, bitidx, 12); // SSRZ GNSS ID Bit Mask
			   for (i = 0; i < cntBits(ZDF012); i++ ) {
			   	ZDF013 = getIoB(bitArray, bitidx, 13); // SSRZ SSRZ Maximum Satellite ID per GNSS and Group
			   	SysArr.push_back(ZDF013);
			   	dbg("\nZMB011.%d->ZDB017.%d ZDF013=%d ",k+1,i+1,SysArr[i]);
			   }
			   dbg("SysArr[%d] ",SysArr.size());
			   ZDF014 = getIoB(bitArray, bitidx, 14); // Satellite Group Definition Mode
			   if (ZDF014 == 0) {
			   	for (i = 0; i < cntBits(ZDF012); i++ ) {
				   	ZDF015 = getIoB(bitArray, bitidx, 15, SysArr[i]); // Satellite Group Bit Mask per GNSS
				   	dbg("\nZMB011.%d->ZDF015.%d=%d ",k+1,i+1,ZDF015);
				}
			   }
		}
	}
}

/* ZMB013 SSRZ Grid Definition Metadata Message Block
   STATUS: UNTESTED!							*/
void procZMB013(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	//int ZDF091, ZDF092, ZDF093, ZDF102, i;
	int    ZDF091, ZDF092, ZDF093, ZDF094, ZDF095, ZDF096, ZDF097;
	int    ZDF098, ZDF099, ZDF100, ZDF101, ZDF102, ZDF103, ZDF104, ZDF105, ZDF106, ZDF303, ZDF304, ZDF305;
	int    i,k,l;
	int_fast64_t lat,lon,chnpnt,aheight,dheight;

	ZDF091 = getIoB(bitArray, bitidx,  91); // Number of grids
	if (ZDF020 == 4) {
		ZDF092 = getIoB(bitArray, bitidx,  92); // Order part of the grid point coordinate resolution
		ZDF093 = getIoB(bitArray, bitidx,  93); // Integer part of the grid point coordinate resolution
		ZDF102 = getIoB(bitArray, bitidx, 102); // Grid point height resolution
	}
	// ZDB012 SSRZ Grid Definition Block 
	   ZDF091 = getIoB(bitArray, bitidx, 91); // Number of grids
	   if (ZDF020 == 4) {
		ZDF092 = getIoB(bitArray, bitidx,  92); // Order part of the grid point coordinate resolution
		ZDF093 = getIoB(bitArray, bitidx,  93); // Integer part of the grid point coordinate resolution
		ZDF102 = getIoB(bitArray, bitidx, 102); // Grid point height resolution 
	   }
	   for (i = 0; i < ZDF091; i++) {
		// ZDB012 SSRZ Grid Definition Block
		   ZDF105 = getIoB(bitArray, bitidx, 105);	// Grid ID
		   ZDF106 = getIoB(bitArray, bitidx, 106);	// Grid IOD
		   if (ZDF020 == 3) {
			ZDF092 = getIoB(bitArray, bitidx,  92);	// Order of the grid point coordinate resolution
			ZDF093 = getIoB(bitArray, bitidx,  93);	// Integer part of the grid point coordinate resolution
		   }
		   ZDF094 = getIoB(bitArray, bitidx,  94);	// Number of Chains per Grid
		   // ZDB10 SSRZ Compressed Chain Block
		      for (k = 0; k < ZDF094; k++) { //if (ZDF094 > 1) {
			ZDF095 = getIoB(bitArray, bitidx, 95); // Number of grid points per chain
			ZDF096 = getIoB(bitArray, bitidx, 96); // Grid Bin Size Parameter
			// ZDB001 Rice-encoded integer value - Lat
			   lat = rdZDB001(bitArray, bitidx, ZDF096);
			   lon = rdZDB001(bitArray, bitidx, ZDF096);
			   // dbg("lat=%lld,lon=%lld ",lat,lon);
			ZDF096 = getIoB(bitArray, bitidx, 96); // Grid Bin Size Parameter
			for (l = 0; l < (2*ZDF095-1); l++) {
			   // ZDB001
			   chnpnt=rdZDB001(bitArray, bitidx, ZDF096);
			   // dbg("ZDB001.%u=%lld",l+1,chnpnt);
			}	
			for (l = 0; l < (ZDF095-2); l++) {
			   ZDF097 = getIoB(bitArray, bitidx,  97); // Use Baseline Flag
			   ZDF098 = getIoB(bitArray, bitidx,  98); // Point Position Flag
			   ZDF099 = getIoB(bitArray, bitidx,  99); // Add Baseline Left Flag
			   ZDF100 = getIoB(bitArray, bitidx, 100); // Add Baseline Right Flag
			}
			if (ZDF020 == 3) {
				ZDF101 = getIoB(bitArray, bitidx, 101); // Height Flag
				if (ZDF101 == 1) {
					ZDF102 = getIoB(bitArray,bitidx,102); // Grid point height resolution
				}
			}	
			if (((ZDF020 == 3) && (ZDF101 == 1)) || ((ZDF020 == 4) && (ZDF102 > 0))) {
				ZDF096 = getIoB(bitArray, bitidx,  96); // Grid Bin Size Parameter
				// ZDB001 Absolute height
				   aheight=rdZDB001(bitArray, bitidx, ZDF096);
				if (ZDF095 > 1) {
				   ZDF096 = getIoB(bitArray, bitidx,  96); // Grid Bin Size Parameter
				   // ZDB001 Height differences
				      dheight=rdZDB001(bitArray, bitidx, ZDF096);
				}
				ZDF103 = getIoB(bitArray, bitidx, 103);	// Gridded Data Predictor Points Flag
				if (ZDF103 == 1) {
					// ZDB011 SSRZ Predictor Point Indicator Block
					   ZDF104 = getIoB(bitArray, bitidx, 303); // Predictor Point Indicator for point δa
					   ZDF104 = getIoB(bitArray, bitidx, 303); // Predictor Point Indicator for point δb
					   ZDF104 = getIoB(bitArray, bitidx, 303); // Predictor Point Indicator for point δc
				}
			}
		      }
	   }
}

/* ZMB025 SSRZ Expected Messages Metadata Message Block
   STATUS: UNTESTED!							*/
/* ZM025 Leftover b=40/176
0110100011101110101000110011101000001011001110100001000001100111011110001000000100001111000101010100111101110100001010100110100100001011 */
void procZMB025(const std::vector<bool>& bitArray, size_t& bitidx, int ZDF020) {
	uint_fast32_t ZDF004;
	int row,col,zdf,bit;
	ZDF004 = getIoB(bitArray, bitidx, 4, 25); // SSRZ Expected Messages Metadata Message Block ZMB025
	dbg("\nZMB025 Expected Messages Metadata Message Block\n");
	for (row = 0; row < 6; row++ ) {
		for (col = 0; col < 6 ; col++ ) {
			zdf = row*5+col;
			bit = (ZDF004 >> zdf) & 1;
			fprintf(stderr,"ZDF%3d=%d ",zdf,bit);
		} fprintf(stderr,"\n");
	}
}

// ZM012 SSRZ Metadata Message, repeats until ZDF020 = 0
void procZM012(const std::vector<bool>& bitArray, size_t& bitidx) {
	int j, ZDF005=0, ZDF020, ZDF003, ZDF021, ZDF011, ZDF055, ZDF044, ZDF060, ZDF042, ZDF061, ZDF053, ZDF054, ZDF012, ZDF043, ZDF041;
        uint16_t ZDF016;
	uint32_t ZDF004;

        ZDF005 = getIoB(bitArray, bitidx, 5); // ZDF005 Metadata IOD
        //pmArr.push_back(bitidx);
        ZDF020 = getIoB(bitArray, bitidx, 20);	  // ZDF020 Metadata Tag
        //pmArr.push_back(bitidx);

        // dbg("!! ZM0%2d->ZDF005=%d->ZMB0%d ZDF020=%d \n",12,ZDF005,ZDF005,ZDF020);
        do { // while ( ZDF020 > 0 ) {
		// ZDF005=1 && ZDF020=1 -> ZMB001 SSRZ High Rate Metadata Message Block
		// ZDF005=2 && ZDF020=1 -> ZMB002 SSRZ Low Rate Metadata Message Block
		ZDF003 = getIoB(bitArray, bitidx, 3);	  // ZDF003 Metadata Type Number 
		//pmArr.push_back(bitidx);
		ZDF021 = getIoB(bitArray, bitidx, 21);	  // ZDF041 Number of SSRZ Metadata bits
		//pmArr.push_back(bitidx);
		// ZMB012 CHECKER
		// dbg(">>> ZDF020=%d ZMB012->ZMB%03d[%d] ",ZDF020,ZDF003,ZDF021);
		int bitsused=bitidx;

		if ((ZDF003 == 1) && (ZDF020 == 1)) { 
			procZMB001(bitArray, bitidx); 
		} else if ((ZDF003 == 2) && (ZDF020 == 1 || ZDF020 == 2)) { 
			procZMB002(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 3) && (ZDF020 == 1 || ZDF020 == 2)) { 
			procZMB003(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 4) && (ZDF020 == 2 || ZDF020 == 3)) {
			procZMB004(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 5) && (ZDF020 == 1)) {
			procZMB005(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 6) ) { //&& ZDF020 == 2) {
			procZMB006(bitArray, bitidx, ZDF020);
		} else if ((ZDF003 == 7) && (ZDF020 == 2 || ZDF020 == 3)) { 
			procZMB007(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 8) && (ZDF020 == 1 || ZDF020 == 2)) { 
			procZMB008(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 11) && (ZDF020 == 1 || ZDF020 == 2)) { 
			procZMB011(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 13) && (ZDF020 == 3 || ZDF020 == 4)) { 
			procZMB013(bitArray, bitidx, ZDF020); 
		} else if ((ZDF003 == 25) && (ZDF020 == 1)) { 
			procZMB025(bitArray, bitidx, ZDF020); 
		} else {
			dbg("\nWARNING: ZMB012->Unknown Metadata Block ZMB%03d ZDF020=%d ",ZDF003,ZDF020);
		}
		
		// While Loop
		ZDF020 = getIoB(bitArray, bitidx, 20);	  // ZDF020 Metadata Tag
		//pmArr.push_back(bitidx);
		if (ZDF020 > 0) {
			// dbg("<<< ZMB0%d Z20=%d >>>\n",12, ZDF020);
			ZDF020 += 0;
		} else {
			if (ZDF021 != (bitidx-bitsused)) {
				// dbg("\n!! WARNING !!  ZM%03d used=%d of length=%d\n",ZDF003,bitidx-bitsused,ZDF021);
				// dbg("\n!! BUG: Previous Data Invalid! Correcting Offset =%d\n",ZDF021-(bitidx-bitsused));
				bitidx += ZDF021-(bitidx-bitsused);
			}
		}
	  } while (ZDF020 > 0);
	  
	  /* DEBUG ZM012
	  i=0; j=0; for (bool bit : bitArray) { 
		if ( i == pmArr[j] ) { j++; std::cout << "|"; }
		i++; std::cout << bit; 
	  } std::cout << std::endl; */
}

// ZM013 SSRZ SSRZ Grid Definition Message 
void procZM013(const std::vector<bool>& bitArray, size_t& bitidx) {
	int    ZDF020;
	ZDF020 = getIoB(bitArray, bitidx, 20);     // SSRZ Metadata Tag
	if (ZDF020 == 3 || ZDF020 == 4) {
		procZMB013(bitArray, bitidx, ZDF020);
	}
}

// ZDF050 15 minutes time tag
void procZDF050(const std::vector<bool>& bitArray, size_t& bitidx, int mtnf) {
	int ZDF050;
	ZDF050 = getIoB(bitArray, bitidx, 50);	// SSRZ 15 minutes Time Tag
        // pmArr.push_back(bitidx);
        fprintf(stderr,"** ZM00%d ZDF050=%d -> %2.0f:%02d SSRZ 15m\n",mtnf,ZDF050,15.0*ZDF050/900,ZDF050%60);
}

// ZM001 SSRZ High Rate Correction Message
void procZM001(const std::vector<bool>& bitArray, size_t& bitidx) {
// ZM001->ZDB003[1] dx=0 p=1 s=0 q=1 r=1 v=0 (CHR)     303=s=0 302=b=1 304=q=1 305=r=1
// ZM001->ZDB003[2] dx=2 p=2 s=1 q=0 r=2 v=-2 (RADORB)
// ZDF301 = Scale Factor Indicator     a unsigned int
// ZDF302 = Bin Size Indicator         b 
// ZDB001 (N x Rice encoded Intenger Value)
   // ZDF303 Sign Bit      s
   // ZDF304 Rice quotient q
   // dx = 2^a * dx0(M)
   // p = p0+b
   // dx0 = HRClk->data[1].defres
	int ZDF016, ZDF301, ZDF302, ZDF303, ZDF304, ZDF305, rf;
	auto [ZDF011, timestamp, delta] = getSSRV(11);
	procZDF050(bitArray, bitidx, 1);
	ZDF016 = getIoB(bitArray, bitidx, 16, ZDF011) ; // Satellite Group List Bit Mask
	//pmArr.push_back(bitidx);
	fprintf(stderr,"** ZM00%d->ZDF016=%d SatGrp=",1,ZDF016);
	std::bitset<16> bits(ZDF016);
	std::cout << bits << std::endl;
	// ZDB003 SSRZ Compressed Satellite Parameter Block for High Rate Clock corrections CHR
	ZDF301 = getIoB(bitArray, bitidx, 301); // Scale Factor Indicator
	//pmArr.push_back(bitidx);
	ZDF302 = getIoB(bitArray, bitidx, 302); // Bin Size Indicator
	//pmArr.push_back(bitidx);
	ZDF303 = getIoB(bitArray, bitidx, 303); // Symbol s
	//pmArr.push_back(bitidx);
	ZDF304 = getIoB(bitArray, bitidx, 304); // Symbol q
	//pmArr.push_back(bitidx);
	ZDF305 = getIoB(bitArray, bitidx, 305, ((HRClk.size > 0)?HRClk.data[0].defbin:0)+ZDF302); // Symbol r

	//fprintf(stderr,"HRCLK[%d] ****",HRClk.size);
	// NOTE CHECK IF METADATA ALREADY RUSHED IN ??? NOTE ///
	if (HRClk.size > 0) {
	   	fprintf(stderr,"\n\nZM001 dx0=%d p0=%d ",HRClk.data[0].defres,HRClk.data[0].defbin);
	   	fprintf(stderr,"a=%d b=%d ",ZDF301,ZDF302);
		fprintf(stderr,"dx=2^a*dx0=2^%d*%d=%d ",ZDF301,HRClk.data[0].defres,(1<<ZDF301)*HRClk.data[0].defres);
		fprintf(stderr,"p=p0+b=%d+%d=%d ",HRClk.data[0].defbin,ZDF302,HRClk.data[0].defbin+ZDF302);
		fprintf(stderr,"n=2^pq+r=2^%d*%d+%d=%ld ",(HRClk.data[0].defbin+ZDF302),ZDF304,ZDF305,(1<<(HRClk.data[0].defbin+ZDF302))*ZDF304+ZDF305);
		fprintf(stderr,"x=n*dx=%d*%d=%d\n",(1<<(HRClk.data[0].defbin+ZDF302))*ZDF304+ZDF305,(1<<ZDF301)*HRClk.data[0].defres,(1<<ZDF301)*HRClk.data[0].defres*((1<<(HRClk.data[0].defbin+ZDF302))*ZDF304+ZDF305));
	}
	
	//pmArr.push_back(bitidx);
	rf=((ZDF303==0)?1:-1)*((2<<ZDF302)*ZDF304+ZDF305);
	fprintf(stderr,"** ZM00%d->ZDB003[1] dx=%d p=%d s=%d q=%d r=%d v=%d (CHR)\n",1,ZDF301, ZDF302, ZDF303, ZDF304, ZDF305, rf);
	// ZDB003 SSRZ Compressed Satellite Parameter Block for High Rate (Radial) Orbit correction
	ZDF301 = getIoB(bitArray, bitidx, 301); // Scale Factor Indicator
	//pmArr.push_back(bitidx);
	ZDF302 = getIoB(bitArray, bitidx, 302); // Bin Size Indicator
	//pmArr.push_back(bitidx);
	ZDF303 = getIoB(bitArray, bitidx, 303); // Symbol s
	//pmArr.push_back(bitidx);
	ZDF304 = getIoB(bitArray, bitidx, 304); // Symbol q
	//pmArr.push_back(bitidx);
	ZDF305 = getIoB(bitArray, bitidx, 305, ZDF302); // Symbol r
	//pmArr.push_back(bitidx);
	rf=((ZDF303==0)?1:-1)*((2<<ZDF302)*ZDF304+ZDF305);
	fprintf(stderr,"** ZM00%d->ZDB003[2] dx=%d p=%d s=%d q=%d r=%d v=%d (RADORB)\n",1,ZDF301, ZDF302, ZDF303, ZDF304, ZDF305, rf);
}

// ZM002 SSRZ Low Rate Correction Message 
void procZM002(const std::vector<bool>& bitArray, size_t& bitidx) {
	int ZDF005, ZDF006, ZDF016, ZDF018, ZDF309;
	procZDF050(bitArray, bitidx, 4); 	// 15 minutes time tag
	ZDF005 = getIoB(bitArray, bitidx, 5); 	// Metadata IOD
	ZDF006 = getIoB(bitArray, bitidx, 6); 	// Metadata Announcement Bit

	// requires ZMB012, present if ZFD020 in ZMB012 is 1!
	// ZDF050
	ZDF016 = getIoB(bitArray, bitidx, 16); 	// Satellite Group List Bit Mask
	ZDF018 = getIoB(bitArray, bitidx, 18); 	// Satellite Bit Mask
	ZDF309 = getIoB(bitArray, bitidx, 309); // SSRZ BE IOD, sequence of IODs (ZDF210), acc.to ZDF018
	// ZDB003 ... NOTE TODO
}

// ZM003 SSRZ Gridded Ionosphere Correction Message
// requires metadata of ZMB003
void procZM003(const std::vector<bool>& bitArray, size_t& bitidx) {
	int ZDF105, ZDF106, ZDF016, ZDF315;
	procZDF050(bitArray, bitidx, 4);
	ZDF105 = getIoB(bitArray, bitidx, 105); // Grid ID
	ZDF106 = getIoB(bitArray, bitidx, 106); // Grid IOD
	ZDF016 = getIoB(bitArray, bitidx, 16);  // Satellite Group List Bit Mask
	// ZDB006 Compressed Gridded Data Block - requires number of chain per grid (ZDF094 of ZMB003/ZMB004)
	   // ZDB005 Compressed Chain Data Block
	      ZDF315 = getIoB(bitArray, bitidx, 315); // Predictor Flag
	      // ZDB002 Rice Block
	      // ZDB002 Rice Block
	      // ZDB002 Rice Block
}

// ZM004 SSRZ Gridded Troposphere Correction Message
void procZM004(const std::vector<bool>& bitArray, size_t& bitidx) {
	int ZDF105, ZDF106, ZDF315, ZDF301, ZDF302, ZDF303, ZDF304, ZDF305, rf,i ;
	procZDF050(bitArray, bitidx, 4);
	ZDF105 = getIoB(bitArray, bitidx, 105);	// SSRZ Grid ID
	ZDF106 = getIoB(bitArray, bitidx, 106);	// SSRZ Grid IOD
	fprintf(stderr,"** ZM00%d ID=%d IOD=%d\n",4,ZDF105,ZDF106);
        // ZDB006 -> {ZDB005 { ZDB002... ZDB002} ZDF107
           ZDF315 =  getIoB(bitArray, bitidx, 315);	// SSRZ Predictor Flag
	   fprintf(stderr,"** ZDB006 ZDF315=%d\n",ZDF315);
           for (i = 0; i < (1+ZDF315); i++) { // 1x ZDB002
           	ZDF301 = getIoB(bitArray, bitidx, 301); // Scale Factor Indicator
		ZDF302 = getIoB(bitArray, bitidx, 302); // Bin Size Indicator
		// ZDB001
		   ZDF303 = getIoB(bitArray, bitidx, 303); // Symbol s
		   ZDF304 = getIoB(bitArray, bitidx, 304); // Symbol q
		   ZDF305 = getIoB(bitArray, bitidx, 305, ZDF302); // Symbol r
		   rf=-ZDF303*((2<<ZDF302)*ZDF304+ZDF305);
		   fprintf(stderr,"** ZM00%d->ZDB005.%d dx=%d p=%d s=%d q=%d r=%d v=%d\n",4,i+1,ZDF301, ZDF302, ZDF303, ZDF304, ZDF305, rf);
	    }
}

// ZM005 SSRZ Satellite dependent Regional Ionosphere Correction Message 
// requires ZMB005
void procZM005(const std::vector<bool>& bitArray, size_t& bitidx) {
	int ZDF016;
	procZDF050(bitArray, bitidx, 4);
	ZDF016 = getIoB(bitArray, bitidx, 16);  // Satellite Group List Bit Mask
	// ZDB008
	   // ZDB002 ... ZDB002
}

// ZM006 SSRZ Global VTEC Ionosphere Correction Message
void procZM006(const std::vector<bool>& bitArray, size_t& bitidx) {
	int ZDF330, ZDF331, ZDF303, ZDF304, ZDF305, ival;
	procZDF050(bitArray, bitidx, 4);
	ZDF330 = getIoB(bitArray, bitidx, 330);	// SSRZ 15 VTEC Flag
        fprintf(stderr,"** ZM006 VTC=%d\n",ZDF330);
        if (ZDF330 > 0) {
        	ZDF331 = getIoB(bitArray, bitidx, 331);	// GLobal VTEC bin size indicator
        	// ZDB062 Global VTEC Rice-encoded integer value
		   ZDF304 = getIoB(bitArray, bitidx, 304); // Symbol q (Rice quotient)
		   ZDF303 = getIoB(bitArray, bitidx, 303); // Symbol s (Sign Bit)
		   ZDF305 = getIoB(bitArray, bitidx, 305); // Symbol r (Rice remainder)

		   ival = ((1LL<<ZDF331)*ZDF304+ZDF305)*((ZDF303==0)?1:-1); 
		   //	dbg("s(Z303)=%d p=%d q(Z304)=%d r(Z305)=%d ",ZDF303,binsize_p,ZDF304,ZDF305); 
		dbg("m=s*(2^p*q+r)=%d*(%llu*%llu+%llu)=%lld ",(ZDF303==0)?1:-1,1LL<<ZDF331,ZDF304,ZDF305,ival);
		//return ival;
        }
}

// ZM007 SSRZ Regional Troposphere Correction Message
void procZM007(const std::vector<bool>& bitArray, size_t& bitidx) {
	procZDF050(bitArray, bitidx, 4);

}

// ZM008 SSRZ QIX Bias Message
void procZM008(const std::vector<bool>& bitArray, size_t& bitidx) {
	int i, ZDF320, ZDF020, ZDF012, ZDF013, ZDF015, ZDF190=0, ZDF191, ZDF089, ZDF045, ZDF046, ZDF019, ZDF090;
        ZDF320= getIoB(bitArray, bitidx, 320);	// ZDF320 SSRZ QIX Metadata Flag
        fprintf(stderr,"** ZM008 ZDF320=%d ",ZDF320);
        if ( ZDF320 == 1 ) {
		ZDF320= getIoB(bitArray, bitidx, 320);	// ZDF320 SSRZ QIX Metadata Flag
		fprintf(stderr,"ZDF020=%d ",ZDF020);
	} else { ZDF020 = 0; }
	if ( ZDF320 == 1 && ZDF020 > 0 ) { // ZMB008
		if (ZDF020 == 2) {
	      		ZDF190 = getIoB(bitArray, bitidx, 190);	// ZDF190 QIX Code Bias flag
	                ZDF191 = getIoB(bitArray, bitidx, 191);	// ZDF191 QIX Phase Bias flag
		}
		if (ZDF020 == 1 || (ZDF020 == 2 && ZDF190 == 1)) {
			ZDF089 = getIoB(bitArray, bitidx, 89);	// ZDF089 QIX Code Bias Default Resolution 
			ZDF045 = getIoB(bitArray, bitidx, 45);	// ZDF045 Number of QIX Code Bias Rice Blocks
                        // ZDB022 -> ZDB020 + ZDF046
                           ZDF012 = getIoB(bitArray, bitidx, 12);	// ZDF012 GNSS ID Bit Mask
                           for (i = 0; i < 16; i++) {
			   	if ( ((ZDF012>>1)&1) == 1) {
  	                        	ZDF019 = getIoB(bitArray, bitidx, 19);	// ZDF019 Signal Bit Mask per GNSS
	                        }
	                   }
	                ZDF046 = getIoB(bitArray, bitidx, 46);	// ZDF046 Default Bin Size Parameter of encoded Signal Biases
	                ZDF090 = getIoB(bitArray, bitidx, 90);	// ZDF090 Phase Bias Default Resolution
		     	ZDF045 = getIoB(bitArray, bitidx, 45);	// ZDF045 Number of QIX Code Bias Rice Blocks
                        // ZDB022
	                   ZDF012 = getIoB(bitArray, bitidx, 12);	// ZDF012 GNSS ID Bit Mask
           	           for (i = 0; i < 16; i++) {
	                   	if ( ((ZDF012>>1)&1) == 1) {
  	                        	ZDF019 = getIoB(bitArray, bitidx, 19);	// ZDF019 Signal Bit Mask per GNSS
	                        }
	                   } 
			ZDF046 = getIoB(bitArray, bitidx, 46);	// ZDF046 Default Bin Size Parameter of encoded Signal Biases
	      }}
	    //} else { ZDF190 = 0; }
	      if ( ZDF020 > 0 ) {
	            ZDF012 = getIoB(bitArray, bitidx, 12);	// ZDF012 SSRZ GNSS ID Bit Mask
	            ZDF013 = getIoB(bitArray, bitidx, 13);	// ZDF013 SSRZ Maximum Satellite ID per GNSS and Group
	            if ( ZDF020 == 1 || (ZDF020 == 2 && ZDF190 == 1)) { // 2 x ZDB004
	            // ZDB002 ... ZDB002
	            }
	      }
	      

}

// ZM009 Time Tag Message
void procZM009(const std::vector<bool>& bitArray, size_t& bitidx) {
	    int ZDF230,ZDF231,ZDF050,ZDF232;
	    procZDF050(bitArray, bitidx, 9);
	    ZDF230= getIoB(bitArray, bitidx, 230);	// ZDF230
	    //pmArr.push_back(bitidx);
	    if        ( ZDF230 == 0) { // SSRZ 1hour - 30seconds Time Tag
		  ZDF231= getIoB(bitArray, bitidx, 231);
		  //pmArr.push_back(bitidx);
		  fprintf(stderr,"** ZM009 ZDF230=%d ZDF231=%d -> %.2f\n",ZDF230,ZDF231,60.0*ZDF231/3600);
	    } else if ( ZDF230 == 1) { // SSRZ 1hour - 5seconds Time Tag
		  ZDF232= getIoB(bitArray, bitidx, 232);	
		  //pmArr.push_back(bitidx);
		  fprintf(stderr,"** ZM009 ZDF230=%d ZDF232=%d -> %.2f\n",ZDF230,ZDF232,60.0*ZDF232/3600);
	    } else if ( ZDF230 == 2) { // SSRZ 15 minutes Time Tag 
		  ZDF050= getIoB(bitArray, bitidx, 50);	
		  //pmArr.push_back(bitidx);
		  fprintf(stderr,"** ZM009 ZDF230=%d ZDF050=%d -> %.2f\n",ZDF230,ZDF050,15.0*ZDF050/900);
	    } else if ( ZDF230 == 88) {
		  ZDF050= getIoB(bitArray, bitidx, 50);	
		  //pmArr.push_back(bitidx);
		  fprintf(stderr,"** ZM009 ZDF230=%d ZDF050=%d -> %f\n",ZDF230,ZDF050,15.0*ZDF050/900);
	    } else {
		  fprintf(stderr,"** ZM009 ZDF230=%d tbd \n",ZDF230);
	    }
}

static void sighandler (int signum) {
	fprintf (stderr, "Signal caught, terminating!\n");
	run. store (false);
}

static
void	syncsignal_Handler (bool b, void *userData) {
	timeSynced. store (b);
	timesyncSet. store (true);
	(void)userData;
}
//
//	This function is called whenever the dab engine has taken
//	some time to gather information from the FIC bloks
//	the Boolean b tells whether or not an ensemble has been
//	recognized, the names of the programs are in the 
//	ensemble
static
void	ensemblename_Handler (const char * name, int Id, void *userData) {
	fprintf (stderr, "ensemble %s is (%X) recognized\n",
	                          name, (uint32_t)Id);
	ensembleRecognized. store (true);
}

std::vector<std::string> programNames;
std::vector<int> programSIds;

static
void	programname_Handler (const char *s, int SId, void *userdata) {
	for (std::vector<std::string>::iterator it = programNames.begin();
	             it != programNames. end(); ++it)
	   if (*it == std::string (s))
	      return;
	programNames. push_back (std::string (s));
	programSIds . push_back (SId);
	fprintf (stderr, "program %s is part of the ensemble\n", s);
}

static
void	programdata_Handler (audiodata *d, void *ctx) {
	(void)ctx;
	fprintf (stderr, "\tstartaddress\t= %d\n", d -> startAddr);
	fprintf (stderr, "\tlength\t\t= %d\n",     d -> length);
	fprintf (stderr, "\tsubChId\t\t= %d\n",    d -> subchId);
	fprintf (stderr, "\tprotection\t= %d\n",   d -> protLevel);
	fprintf (stderr, "\tbitrate\t\t= %d\n",    d -> bitRate);
}

//
//	The function is called from within the library with
//	a string, the so-called dynamic label
static
void	dataOut_Handler (const char * dynamicLabel, void *ctx) {
	(void)ctx;
//	fprintf( stderr, "DBG: dataOut_Handler invoked.\n");
	fprintf (stderr, "DynLabel= %s\n", dynamicLabel);
}
//
//	Note: the function is called from the tdcHandler with a
//	frame, either frame 0 or frame 1.
//	The frames are packed bytes, here an additional header
//	is added, a header of 8 bytes:
//	the first 4 bytes for a pattern 0xFF 0x00 0xFF 0x00 0xFF
//	the length of the contents, i.e. framelength without header
//	is stored in bytes 5 (high byte) and byte 6.
//	byte 7 contains 0x00, byte 8 contains 0x00 for frametype 0
//	and 0xFF for frametype 1
//	Note that the callback function is executed in the thread
//	that executes the tdcHandler code.
static
void	bytesOut_Handler (uint8_t *data, int16_t amount,
	                  uint8_t type, void *ctx) {
//	fprintf( stderr, "DBG: bytesOut_Handler = %d bytes.\n", amount);
#ifdef DATA_STREAMER
uint8_t localBuf [amount + 8];
int16_t i;
std::vector<int> pmArr;

	localBuf [0] = 0xFF;
	localBuf [1] = 0x00;
	localBuf [2] = 0xFF;
	localBuf [3] = 0x00;
	localBuf [4] = (amount >> 8) & 0xFF;
	localBuf [5] = amount & 0xFF;
	localBuf [6] = 0x00;
	localBuf [7] = type == 0 ? 0 : 0xFF;
//	// dbg("** DBG BYTESOUT = ");
	for (i = 0; i < amount; i ++) {
           localBuf [8 + i] = data[i]; // data;
           //// dbg("%02x",data[i]);
	} // // dbg("\n");
	tdcServer. sendData (localBuf, amount + 8);
	if ( data[0] != 0xd3 ) dbg("DATA ERROR");

/* NOTE SSRZ header (consisting of RTCM3 message type DF002 and 
        SSRZ sub-type ZDF000) */

	//uint8_t msgtyp = data[2]; 	// Definitiv 8-bit Intenger ....... ZDF004 SSRZ Message ID Bit Mask -> ZDF020 -> ZMB025 -> 25bits? -> 3 Byte +1 Bit?
	//int8_t  dx0; // = data[3], a;  	// a,b,Ndata are part of SSRZ Rice-Block
					// This parameter is dynamically derived from metadata settings and the number of 
					// supported satellites, respectively.
	std::vector<bool> bitArray = byteArr2Bits(&data[1], amount-1 ); //amount-2);
	uint8_t  ZDF002, sob, trash=0;
	size_t  j, bitidx = 0;

	uint16_t mtna  = getIoB(bitArray, bitidx, 0, 4,-1,0);	// Always 0?
	pmArr.push_back(bitidx);
	uint16_t mtnb  = getIoB(bitArray, bitidx, 0,12,-1,0);	// Länge in Bytes (+40 = Länge des Datensatzes)
	pmArr.push_back(bitidx);
	uint16_t mtnc  = getIoB(bitArray, bitidx, 0, 1,-1,0);	// Some Bit? 
	pmArr.push_back(bitidx);
	uint16_t mtnd  = getIoB(bitArray, bitidx, 0,12,-1,0);	// 111111110100 vs. 111111101100 RTCM3 DF002 header
	pmArr.push_back(bitidx);
	uint16_t mtne  = getIoB(bitArray, bitidx, 0, 3,-1,0);	// Some Bit(s)? 
	pmArr.push_back(bitidx);
	trash += mtna+mtnb+mtnc+mtnd+mtne+sob;

	//mtnf  = getIoB(bitArray, bitidx, 0, 2, 1,5);	// ZDF002 PC(2,1,5) SSRZ Message Type Number
	ZDF002  = getIoB(bitArray, bitidx, 2); 	// ZDF002 Message Type Number
	sob=bitidx;
	pmArr.push_back(bitidx);

	// NOTE DEBUG 
	// // dbg("** a=%02x%02x%02x a=%d b=%d/%d l=%d c=%d d=%d e=%d f=%d|",data[0],data[1],data[2],mtna,bitidx,mtnb*8+40,bitArray.size(),mtnc,mtnd,mtne,mtnf);	
	dbg("** LBy=%4d LBi=%4d ZM%03d bi=%d **\n", mtnb*8+40,bitArray.size(),ZDF002,bitidx);

	/* DEBUG PRINT BITFIELD
	   i=0; j=0; for (bool bit : bitArray) { 
		if ( i == pmArr[j] ) { j++; std::cout << "|"; }
		if ( i == bitidx   ) { std::cout << "#"; }
		i++; std::cout << bit; 
	} std::cout << std::endl;  */
	
	// SSRZ Metadata Messages
	if (ZDF002 == 11) { 		// ZM011 SSRZ Satellite Group Definition Message
	    procZM011(bitArray, bitidx);
	} else if (ZDF002 == 12) {	// ZM012 SSRZ Metadata Message
	    procZM012(bitArray, bitidx);
	} else if (ZDF002 == 13) {	// ZM013 SSRZ Grid Definition Message 
	    procZM013(bitArray, bitidx);
	// SSRZ Correction Messages
	// ZM001 SSRZ High Rate Correction Message
	// ZM002 SSRZ Low Rate Correction Message
	// ZM003 SSRZ Gridded Ionosphere Correction Message 
	// ZM004 SSRZ Gridded Troposphere Correction Message
	// ZM005 SSRZ Satellite dependent Regional Ionosphere Correction Message
	// ZM006 SSRZ Global VTEC Ionosphere Correction Message
	// ZM007 SSRZ Regional Troposphere Correction Message
	// ZM008 SSRZ QIX Bias Message
	// ZM009 SSRZ Time Tag Message
	} else if (ZDF002 == 1) {
	    procZM001(bitArray, bitidx);
	} else if (ZDF002 == 2) {
	    procZM002(bitArray, bitidx);
	} else if (ZDF002 == 3) {
	    procZM003(bitArray, bitidx);
	} else if (ZDF002 == 4) {
	    procZM004(bitArray, bitidx);
	} else if (ZDF002 == 5) {
	    procZM005(bitArray, bitidx);
	} else if (ZDF002 == 6) {
	    procZM006(bitArray, bitidx);
	} else if (ZDF002 == 7) {
	    procZM007(bitArray, bitidx);
	} else if (ZDF002 == 8) {
	    procZM008(bitArray, bitidx);
	} else if (ZDF002 == 9) {
	    procZM009(bitArray, bitidx);
	}
	
	// DEBUG: Print Rest of field 
	dbg("ZM%03d Leftover b=%d/%d\n",ZDF002,bitidx,bitArray.size());
	for (j = bitidx; j < bitArray.size(); j++) { fprintf(stderr,"%d",bitArray[j]&1); } // fprintf(stderr,"");
	ZDF002 = getIoB(bitArray, bitidx, 2);     // SSRZ Metadata Tag
	dbg("\nNext ZDF002 = %02d?\n", ZDF002);


// NOTE DO NOT DELETE !! NOTE //
#else
	(void)data;
	(void)amount;
#endif
	(void)ctx;
}

/* NOTE - SSRZ Metadata & corrections 
	- Satellite Group List 
	- Metadata 
	  - The Default Resolution 𝒅𝒙𝟎 and 
	  - the Default Bin Size Parameter 𝒑𝟎 are static (Default Rice) parameters and will be part of the SSRZ metadata.
	  - Both the SSRZ Resolution Indicator 𝑎 and 
	  - the SSRZ Bin Size Indicator 𝑏 are dynamical/adaptive parameters. 
	  - Together with the Ndata encoded data, they compose a so-called SSRZ Rice-Block (Table 3.1).
	  ! The Number of data values to be compressed (Ndata) must be known. 
	  ! This parameter is dynamically derived from metadata settings and the number of supported satellites, respectively.

	  3.1 SSRZ Rice Block
	  |SSRZ Metadata (static)|SSRZ Rice Block              |
	  |Defailt Rice Parameter|Adaptive Rice      |Ndata...
	  |Default |Default Bin  |Scale Fact|Bin Size|
	  |res. 𝒅𝒙𝟎|Size 𝒑𝟎      |Indic a   |Indic b |
	  
	  3.2 SSRZ Compressed Satellite Parameter Block
	  + Additional Columns for SAT System in front of Default Rice Parameters
	  
	  3.3 SSRZ Compressed Signal BiaS Block
	  + Additional Columns for frequency and GNSS
	  
	  3.4 SSRZ Compressed Chain Data Block
	  
	  ZDBxxx -> Grouped of Data Fields
	  
	  SSRZ Corrections Messages
	  ZM001 High Rate Correction Message
	  ZM002 Low Rate Correction Message
	  ZM003 Gridded Ionosphere Correction Message
	  ZM004 Gridded Troposhere Correction Message
	  ZM005 Satellite dependent Regional Ionosphere Correction Message
	  ZM006 Global VTEC Ionosphere Correction Message
	  ZM007 Regional Troposphere Correction Message
	  ZM008 QIX Bias Message
	  ZM009 Time Tag Message
	  ZM010 ??
	  SSRZ Metadata Messages
	  ZM011 Satellite Group Definition Message
	  ZM012 Metadata Message
	  ZM013 Grid Definition Message
	  ZM025 Expected SSRZ Messages
	  

  NOTE CLK93 stream equivalent
        RTCM occ Parameter Nature
        1060   5 GPS orbits/clocks
        1066   5 GLONASS orbits/clocks
        1243   5 GALILEO orbits/clocks
        1059   5 GPS code biases
        1065   5 GLONASS code biases
        1242   5 GALILEO code biases
        1265   5 GPS phase biases L1,L2
        1265   5 GPS phase biases L5
        1264  60 Ionosphere VTEC 

   NOTE   59   ? GeoPP FKP-AdV / SAPOS Flächenkorrekturparameter -> 1-3cm in 20s
   NOTE 4090   ? Geo++ Proprietary Message; Sub-Type 7 für Geo++ SSRZ data
                 -> RTCM3 framing (3 Byte Header, incl. msg lenth + 3 byte checksum
                 - SSRZ header sub-type ZDF000
                 - Geo++ SSRG messages? in Geo++ SSRZ format??
   
   RTCM MSM4 kontinuierlich aus Geo++ ss2obs?
   
   NOTE https://software.rtcm-ntrip.org/export/HEAD/ntrip/trunk/BNC/src/bnchelp.html
   RTCM SSR I									
1057	Orbit Corrections	GPS		x	x	x	x	x	x
1063	Orbit Corrections	GLONASS		x	x	x	x	x	x
1240	Orbit Corrections	Galileo	x	x	x	x	x	x	x
1246	Orbit Corrections	SBAS	x	x	x	x	x		x
1252	Orbit Corrections	QZSS	x	x	x	x	x		x
1258	Orbit Corrections	BDS	x	x	x	x	x	x	x
1058	Clock Corrections	GPS		x	x	x	x	x	x
1064	Clock Corrections	GLONASS		x	x	x	x	x	x
1241	Clock Corrections	Galileo	x	x	x	x	x	x	x
1247	Clock Corrections	SBAS	x	x	x	x	x		x
1253	Clock Corrections	QZSS	x	x	x	x	x		x
1259	Clock Corrections	BDS	x	x	x	x	x	x	x
1059	Code Biases	GPS		x	x	x	x	x	x
1065	Code Biases	GLONASS		x	x	x	x	x	x
1242	Code Biases	Galileo	x	x	x	x	x	x	x
1248	Code Biases	SBAS	x	x	x	x	x		x
1254	Code Biases	QZSS	x	x	x	x	x		x
1260	Code Biases	BDS	x	x	x	x	x	x	x
1061, 1062	User Range Accuracy, HR 	GPS		x					
1067, 1068	User Range Accuracy, HR 	GLONASS		x					
1244, 1245	User Range Accuracy, HR 	Galileo	x	x					
1250, 1251	User Range Accuracy, HR 	SBAS	x	x					
1256, 1257	User Range Accuracy, HR 	QZSS	x	x					
1262, 1263	User Range Accuracy, HR 	BDS	x	x					
1060	Comb. Orbits & Clocks	GPS		x	x	x	x	x	x
1066	Comb. Orbits & Clocks	GLONASS		x	x	x	x	x	x
1243	Comb. Orbits & Clocks	Galileo	x	x	x	x	x	x	x
1249	Comb. Orbits & Clocks	SBAS	x	x	x	x	x		x
1255	Comb. Orbits & Clocks	QZSS	x	x	x	x	x		x
1261	Comb. Orbits & Clocks	BDS	x	x	x	x	x	x	x
RTCM SSR II									
1264	VTEC	GNSS	x	x	x	x	x	x	
1265	Phase Biases	GPS	x	x	x	x	x	x	
1266	Phase Biases	GLONASS	x	x	x	x	x	x	
1267	Phase Biases	Galileo	x	x	x	x	x	x	
1268	Phase Biases	SBAS	x	x	x	x	x		
1269	Phase Biases	QZSS	x	x	x	x	x		
1270	Phase Biases	BDS	x	x	x	x	x	x	

IGS SSR									
4076	IGS SSR	GNSS		x	x	x	x	x	x
   NOTE https://software.rtcm-ntrip.org/wiki/SSRProvider
   */
        


//
//	This function is overloaded. In the normal form it
//	handles a buffer full of PCM samples. We pass them on to the
//	audiohandler, based on portaudio. Feel free to modify this
//	and send the samples elsewhere
//
//	However, in the "special mode", the aac frames are send out
//	Obviously, the parameters "rate" and "isStereo" are meaningless
//	then.
static
void	pcmHandler (int16_t *buffer, int size, int rate,
	                              bool isStereo, void *ctx) {
static bool isStarted	= false;
//	fprintf( stderr, "DBG: pcmHandler invoked with %d.\n", size);
	(void)isStereo;
	if (!isStarted) {
	   soundOut	-> restart ();
	   isStarted	= true;
	}
	soundOut	-> audioOut (buffer, size, rate);
}

static
void	systemData (bool flag, int16_t snr, int32_t freqOff, void *ctx) {
//	fprintf (stderr, "synced = %s, snr = %d, offset = %d\n",
//	                    flag? "on":"off", snr, freqOff);
}

static
void	fibQuality	(int16_t q, void *ctx) {
	//fprintf (stderr, "fic quality = %d\n", q);
}

/*      The quality of the DAB data is reflected in 1 number in case
//	of DAB, and 3 in case of DAB+,
//	the first number indicates the percentage of dab packages that
//	passes tests, and for DAB+ the percentage of valid DAB_ frames.
//	The second and third number are for DAB+: the second gives the
//	percentage of packages passing the Reed Solomon correction,
//	and the third number gives the percentage of valid AAC frames
*/
static
void	mscQuality	(int16_t fe, int16_t rsE, int16_t aacE, void *ctx) {
	fprintf (stderr, "msc quality fe=%d rsE=%d aacE=%d\n", fe, rsE, aacE);
}


WINDOW *create_newwin(int height, int width, int starty, int startx)
{
    WINDOW *local_win;
    local_win = newwin(height, width, starty, startx);
    box(local_win, 0 , 0);              /* 0, 0 gives default characters 
                                           for the vertical and horizontal
                                            lines                       */
    wrefresh(local_win);                /* Show that box                */
    return local_win;
}


int	main (int argc, char **argv) {
// Default values
uint8_t		theMode		= 1;
std::string	theChannel	= "5C"; //11C
uint8_t		theBand		= BAND_III;
int16_t		ppmCorrection	= -2; // BLOG V4
// BLOG V3 -3;   // -1 -> 427 // -2 -> 208 // -3 -> -14 // -4 -> -231

#ifdef  HAVE_SDRPLAY
int16_t		GRdB		= 30;
int16_t		lnaState	= 2;
#else
int		theGain		= 18;	// 35 scale = 0 .. 100
#endif
std::string	soundChannel	= "default";
int16_t		latency		= 10;
int16_t		timeSyncTime	= 10; //5
int16_t		freqSyncTime	= 10;
int16_t		dataSyncTime	= 15;
bool		autogain	= false; // true; // false;
int	opt;
struct sigaction sigact;
bandHandler	dabBand;
deviceHandler	*theDevice;
#ifdef	HAVE_WAVFILES
std::string	fileName;
#elif	HAVE_RAWFILES
std::string	fileName;
#elif HAVE_RTL_TCP
std::string	hostname = "127.0.0.1";		// default
int32_t		basePort = 1234;		// default
#endif
bool	err;

	/* DEMO Template stuff
	/
	ParameterTuple parameters = getDefaultParameters(2);
	int N0 = std::get<0>(parameters);
	int db = std::get<1>(parameters);
	int Nmax = std::get<2>(parameters);
	std::string zdf = std::get<3>(parameters);
	dbg("N0=%d db=%d Nmax=%d data=%s\n",N0,db,Nmax,zdf.c_str()); // std::cout << "String Value: " << stringValue << std::endl;
	
	int value = 2;
	int decodedValue = 42;
	storeSSRV(value, decodedValue);

	// Retrieve the decoded value and timestamp
	SSRValue retrievedValue = getSSRVo(value);

	std::cout << "Value: " << value << std::endl;
	std::cout << "Decoded Value: " << retrievedValue.value << std::endl;

	// Calculate the time difference
	std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
	std::chrono::duration<double> deltaa = currentTime - retrievedValue.timestamp;
	std::cout << "Delta: " << deltaa.count() << " seconds" << std::endl;
	
	auto [ZDF02, timestamp, delta] = getSSRV(value);

	std::cout << "Value: " << value << std::endl;
	std::cout << "Decoded Value: " << ZDF02 << std::endl;
	std::cout << "Timestamp: " << std::chrono::system_clock::to_time_t(timestamp) << std::endl;
	std::cout << "Delta: " << delta << " seconds" << std::endl;*/

	// Allocate a buffer
	// char buffer[MAX_BUFFER_SIZE];
	// memset(buffer, 0, sizeof(buffer)); // Initialize the buffer to avoid garbage data
	// uint8_t* uint8_buffer = reinterpret_cast<uint8_t*>(buffer);
	//  dbgServer. sendData (buffer, sizeof(buffer));
	// Now you can use 'uint8_buffer' to send data via TCP or perform other operations
	// dbgServer.sendData(uint8_buffer, sizeof(buffer));

	dbg("dab_cmdline V 1.0alfa example 5,\n \
	                  Copyright 2017 J van Katwijk, Lazy Chair Computing\n");
	dbg("** SSRZ via PPP-RTK-AdV via Channel 5C by Stefan Juhl\n");
#ifdef	DATA_STREAMER
	dbg("** Built-in TDC Server @port 8888 .. ");
#endif
	// IMPORTANT Initialisation block
	InitZDFP(); 			// Initalize ZDF Parameter Block

	timeSynced.	store (false);
	timesyncSet.	store (false);
	run.		store (false);

	if (argc == 1) {
	   printOptions ();
	   exit (1);
	}

//	For file input we do not need options like Q, G and C,
//	We do need an option to specify the filename
#if	(defined (HAVE_WAVFILES) && defined (HAVE_RAWFILES))
	while ((opt = getopt (argc, argv, "D:d:M:B:P:A:L:S:F:O:")) != -1) {
#elif   HAVE_RTL_TCP
	while ((opt = getopt (argc, argv, "D:d:M:B:C:P:G:A:L:S:H:I:QO:")) != -1) {
#else
	while ((opt = getopt (argc, argv, "D:d:M:B:C:P:G:A:L:S:QO:")) != -1) {
#endif

	   switch (opt) {
	      case 'D':
	         freqSyncTime	= atoi (optarg);
	         break;

	      case 'd':
	         timeSyncTime	= atoi (optarg);
	         break;

	      case 'M':
	         theMode	= atoi (optarg);
	         if (!((theMode == 1) || (theMode == 2) || (theMode == 4)))
	            theMode = 1; 
	         break;

	      case 'B':
	         theBand = std::string (optarg) == std::string ("L_BAND") ?
	                                     L_BAND : BAND_III;
	         break;

	      case 'P':
	         programName	= optarg;
	         break;

	      case 'p':
	         ppmCorrection	= atoi (optarg);
	         break;
#if defined (HAVE_WAVFILES) || defined (HAVE_RAWFILES)
	      case 'F':
	         fileName	= std::string (optarg);
	         break;
#else
	      case 'C':
	         theChannel	= std::string (optarg);
	         break;

#ifdef	HAVE_SDRPLAY
	      case 'G':
	         GRdB		= atoi (optarg);
	         break;

	      case 'L':
	         lnaState	= atoi (optarg);
	         break;

#else
	      case 'G':
	         theGain	= atoi (optarg);
	         break;

	      case 'L':
	         latency	= atoi (optarg);
	         break;
#endif

	      case 'Q':
	         autogain	= true;
	         break;

#ifdef	HAVE_RTL_TCP
	      case 'H':
	         hostname	= std::string (optarg);
	         break;

	      case 'I':
	         basePort	= atoi (optarg);
	         break;
#endif
#endif

	      case 'O':
	         soundOut	= new fileSink (std::string (optarg), &err);
	         if (!err) {
	            fprintf (stderr, "sorry, could not open file\n");
	            exit (32);
	         }
	         break;

	      case 'A':
	         soundChannel	= optarg;
	         break;

	      default:
	         printOptions ();
	         exit (1);
	   }
	}
//
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;

	int32_t frequency	= dabBand. Frequency (theBand, theChannel);
	try {
#ifdef	HAVE_SDRPLAY
	   theDevice	= new sdrplayHandler (frequency,
	                                      ppmCorrection,
	                                      GRdB,
	                                      lnaState,
	                                      autogain,
	                                      0,
	                                      0);
#elif	HAVE_AIRSPY
	   theDevice	= new airspyHandler (frequency,
	                                     ppmCorrection,
	                                     theGain, false);
#elif	HAVE_RTLSDR
	   theDevice	= new rtlsdrHandler (frequency,
	                                     ppmCorrection,
	                                     theGain,
	                                     autogain);
#elif	HAVE_WAVFILES
	   theDevice	= new wavFiles (fileName);
#elif	HAVE_RAWFILES
	   theDevice	= new wavFiles (fileName);
#elif	HAVE_RTL_TCP
	   theDevice	= new rtl_tcp_client (hostname,
	                                      basePort,
	                                      frequency,
	                                      theGain,
	                                      autogain,
	                                      ppmCorrection);
#endif

	}
	catch (int e) {
	   fprintf (stderr, "allocating device failed (%d), fatal\n", e);
	   exit (32);
	}
//
	if (soundOut == NULL) {	// not bound to a file?
	   soundOut	= new audioSink	(latency, soundChannel, &err);
	   if (err) {
	      fprintf (stderr, "no sound output\n");
	      // fprintf (stderr, "no valid sound channel, fatal\n");
	      // exit (33);
	   }
	}
//
//	and with a sound device we can create a "backend"
	API_struct interface;
        interface. dabMode      = theMode;
        interface. syncsignal_Handler   = syncsignal_Handler;
        interface. systemdata_Handler   = systemData;
        interface. ensemblename_Handler = ensemblename_Handler;
        interface. programname_Handler  = programname_Handler;
        interface. fib_quality_Handler  = fibQuality;
        interface. audioOut_Handler     = pcmHandler;
        interface. dataOut_Handler      = dataOut_Handler;
        interface. bytesOut_Handler     = bytesOut_Handler;
        interface. programdata_Handler  = programdata_Handler;
        interface. program_quality_Handler = mscQuality;
        interface. motdata_Handler      = nullptr;
        interface. tii_data_Handler     = nullptr;;
        interface. timeHandler		= nullptr;

	theRadio	= dabInit (theDevice,
	                           &interface,
	                           NULL,		// no spectrum shown
	                           NULL,		// no constellations
	                           NULL
	                          );
	if (theRadio == NULL) {
	   fprintf (stderr, "sorry, no radio available, fatal\n");
	   exit (4);
	}

//	theDevice	-> setGain (theGain);
	if (autogain)
	   theDevice	-> set_autogain (autogain);
	theDevice	-> restartReader (frequency);
//
//	The device should be working right now

	timesyncSet.		store (false);
	ensembleRecognized.	store (false);
	dabStartProcessing (theRadio);

	fprintf (stderr, "AutoGain=");
	if (autogain) { fprintf(stderr,"Y\n");
	} else { fprintf(stderr,"N\n"); }
	fprintf (stderr, "TimeSync: ");
	while (!timeSynced. load () && (--timeSyncTime >= 0)) {
	   fprintf (stderr, "-%d-\r\r\r", timeSyncTime);
	   sleep (1);
	}
	fprintf (stderr, "\n");

	if (!timeSynced. load () && ensembleRecognized. load ()) {
	   cerr << "There does not seem to be a DAB signal here" << endl;
	   theDevice -> stopReader ();
	   sleep (1);
	   dabStop	(theRadio);
	   dabExit	(theRadio);
	   delete theDevice;
	   exit (22);
	}
	else
	   cerr << "there might be a DAB signal here" << endl;

	while (!ensembleRecognized. load () &&
	                             (--freqSyncTime >= 0)) {
	    fprintf (stderr, "%d\r", freqSyncTime);
	    sleep (3);
	}
	fprintf (stderr, "\n");

	if (!ensembleRecognized. load ()) {
	   fprintf (stderr, "no ensemble data found, fatal\n");
	   theDevice -> stopReader ();
	   sleep (1);
	   dabStop	(theRadio);
	   dabExit	(theRadio);
	   delete theDevice;
	   exit (22);
	}

	run. store (true);
	std::thread keyboard_listener = std::thread (&listener);
        std::cerr << "we try to start program " <<
                                                 programName << "\n";

	/* Handle Data Service */
	std::cerr << "** DBG: Packet Data Check @" << programName << endl;
	if (is_dataService (theRadio, programName. c_str ())) {
           packetdata pd;      
           dataforDataService (theRadio, programName. c_str (), &pd, 0);
           
           fprintf (stderr, "** DataSync: ");
           if (!pd. defined) {
           	while (!pd. defined && (--dataSyncTime >= 0)) {
	        	fprintf (stderr, "%d\r", dataSyncTime);
		        sleep (1);
		}
	        fprintf (stderr, "\n");
	   } else { fprintf (stderr, "OK\n"); }
           
           if (!pd. defined) {
                std::cerr << "FAIL" << endl;
           	//std::cerr << "sorry  we cannot handle service " <<
		//	     programName << "\n";
		run. store (false);
	   // } else { std::cerr << "DFD: OK\n";
	   }
	   dabReset_msc (theRadio);
           set_dataChannel (theRadio, &pd);

           //run. store (false);
	   //exit (22);
	// } else { std::cerr << "DataChk: NO\n";
	}

/*        if (!is_audioService (theRadio, programName. c_str ())) {
           std::cerr << "sorry  we cannot handle service " <<
                                                 programName << "\n";
           //run. store (false);
	   //exit (22);
        } else {
	   audiodata ad;
	   dataforAudioService (theRadio, programName. c_str (), &ad, 0);
	   if (!ad. defined) {
           	std::cerr << "sorry  we cannot handle service " <<
                                                 programName << "\n";
		run. store (false);
	   }
	   dabReset_msc (theRadio);
           set_audioChannel (theRadio, &ad);
        } */

	/* Keyboard Reader */
	while (run. load ()) {
	   message m;
	   while (!messageQueue. pop (10000, &m));
	   switch (m. key) {
	      case S_NEXT:
	         // selectNext ();
	         packetdata pd;      
	         dataforDataService (theRadio, programName. c_str (), &pd, 0);
	         dabReset_msc (theRadio);
                 set_dataChannel (theRadio, &pd);
	         break;
              case S_TMNG:
                 fprintf(stderr,"You pressed T! :)"); 
	      case S_QUIT:
	         run. store (false);
	         break;
	      default:
	         break;
	   }
	}

	// Cleanup
	// delwin(my_win); // Delete the window
	// endwin(); // Close ncurses*/

	cleanupTC(&SatTimGrp);

	theDevice	-> stopReader ();
	keyboard_listener. join ();
	dabStop	(theRadio);
	dabExit	(theRadio);
	delete theDevice;	
	delete soundOut;
}

void    printOptions (void) {
	fprintf (stderr,
"                          dab-cmdline options are\n\
	                  -W number   amount of time to look for an ensemble\n\
	                  -M Mode     Mode is 1, 2 or 4. Default is Mode 1\n\
	                  -B Band     Band is either L_BAND or BAND_III (default)\n\
	                  -P name     program to be selected in the ensemble\n\
	                  -C channel  channel to be used\n\
	                  -G Gain     gain for device (range 1 .. 100)\n\
	                  -L number   latency for audiobuffer\n\
	                  -G gainreduction for SDRplay\n\
	                  -L lnaState for SDRplay\n\
	                  -Q          if set, set autogain for device true\n\
	                  -F filename in case the input is from file\n\
	                  -A name     select the audio channel (portaudio)\n\
	                  -S hexnumber use hexnumber to identify program\n\n\
	                  -O filename put the output into a file rather than through portaudio\n");
}

bool	matches (std::string s1, std::string s2) {
const char *ss1 = s1. c_str ();
const char *ss2 = s2. c_str ();

	while ((*ss1 != 0) && (*ss2 != 0)) {
	   if (*ss2 != *ss1)
	      return false;
	   ss1 ++;
	   ss2 ++;
	}
	return *ss2 == 0;
}

void	selectNext	(void) {
//int16_t	i;
long unsigned int i;
int16_t	foundIndex	= -1;

	for (i = 0; i < programNames. size (); i ++) {
	   if (matches (programNames [i], programName)) {
	      if (i == programNames. size () - 1)
	         foundIndex = 0;
	      else 
	         foundIndex = i + 1;
	      break;
	   }
	}

	if (foundIndex == -1) {
	   fprintf (stderr, "system error\n");
	   sighandler (9);
	   exit (1);
	}

//	skip the data services. Slightly dangerous here, may be
//	add a guard for "only data services" ensembles
	while (!is_audioService (theRadio,
                                 programNames [foundIndex]. c_str ()))
	   foundIndex = (foundIndex + 1) % programNames. size ();

	programName = programNames [foundIndex];
	fprintf (stderr, "we now try to start program %s\n",
	                                         programName. c_str ());

	audiodata ad;
        dataforAudioService (theRadio, programName. c_str (), &ad, 0);
        if (!ad. defined) {
           std::cerr << "sorry  we cannot handle service " <<
                                                 programName << "\n";
	   sighandler (9);
	}
	dabReset_msc (theRadio);
        set_audioChannel (theRadio, &ad);
}

void	listener	(void) {
	fprintf (stderr, "Keyboard listener is active, press RETURN for retune to %s\n",programName. c_str ());
	while (run. load ()) {
	   char t = getchar ();
	   //char t = getch();
	   message m;
	   switch (t) {
	      case '\n': 
	         m.key = S_NEXT;
	         m. string = "";
	         messageQueue. push (m);
	         break;
	      case 't':
	      	 m.key = S_TMNG;
	      	 m. string = "";
	         messageQueue. push (m);
	         break;
	      /*case 'Q':
	         m.key = S_QUIT;
	         m. string = "";
	         messageQueue. push (m);
	         break;*/
	      default:
	         fprintf (stderr, "unidentified %d (%c)\n", t, t);
	   }
	}
}

