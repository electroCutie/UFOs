#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#define _stride(_type) ({  \
  _type* _t = (_type*) 0;  \
  (uint64_t) (_t+1);       \
  })


typedef void* any_t;


/* Instance */
typedef any_t ufInstance_t;

/**
 * Allocate, set default options, and return an instnce object. This does not intialize the object, which must be done before it is used
 */
ufInstance_t ufMakeInstance();

/**
 * Intialize an instance of UFO. This performs the nessesary work to make the instance useable and must be done beore any objects are created using the instance
 * No configuration changes may be made once the instance is initialized
 */
int ufInit(ufInstance_t instance);

/**
 * Free all resources associated with this instance
 * Will make best efforts to stop issuing fill requests in a timely fashion, but no guarantees are made about the status of resources when this call returns
 * To wait for all resources to be freed call ufAwayShutdown after this returns
 * If free is true then the object will be freed and the user cannot away shutdown, if false then the user must await shuitdown and free the object herself
 */
void ufShutdown(ufInstance_t instance, bool free);

/**
 * Await shutdown of the instance. Will only return once all resources used by this instance are freed. Must be called after ufShutdown returns
 */
void ufAwaitShutdown(ufInstance_t instance);

/**
 * returns the page size for this instance
 * Usually this will exactly equal the page size of the system, and it will never change for an instance
 * For portability one should not rely on this value never changing between runs, however
 */
uint32_t ufGetPageSize(ufInstance_t instance);

/*Populate*/

typedef void* ufUserData;

typedef struct{
  int cmd;
  union{
    struct {
      void*     start;
      uint64_t  lengthBytes;
    } resolve;
    struct {
      uint64_t  requestedLength;
      int       roundingModeRequested;
      uint64_t  grantedLength;
      void*     newTarget;
    } expand;
  };
} ufPopulateCalloutMsg;

#define ufResolveRangeCmd 1
#define ufExpandRange     2

/**
 * Change the semantics of the ongoing ufPopulateRange operation in some way
 *
 * ufPopulateCalloutMsg.resolve, ufResolveRangeCmd
 *    @arg start, pointer to the start of the partial range to resolve
 *    @arg lengthBytes, length, in bytes, to resolve
 *  This is an advisory to UFO that a section of memory has been resolved and can be copied into the program's memory space immediately
 *  Once this call is made the memory from start to start+length must not be written to, even after this call returns
 *  It will be most efficient to call this when lengthBytes is some integer multiple of ufGetPageSize
 *  No guarantees are made about when the copy actually happens, this call is advisory only and may be ignored
 *  The function returns
 *    0                     on success
 *    ufErrAlreadyResolved  if the range contained any part that had already been resolved by a previous call
 *    ufErrOutOfBounds      if the resolve specified any bytes putside of the resolve range
 *
 * ufPopulateCalloutMsg.expand, ufExpandRange
 *    @arg requestedLength, request to expand the range of memory to fill to this size. This may not be smaller than the initial fill length
 *    @arg roundingModeRequested. which way the caller prefers UFO round when deciding how large an area to grant
 *           values are ufRoundUp or ufRoundDown
 *    @return grantedLength, the new size of the memory range to fill. This may not be equal to the requested length but will never be smaller than the initial fill length
 *    @return newTarget, pointer to the new area to fill
 *  Attempts to grow the memory fill. This is meant to allow a program to rake advantage of an expensive operation, such as a disk seek, which yields more blocks of memory than were requested
 *  Memory must be filled one page at a time, so the granted length may not equal the requested length. It is wise to simply request as much as makes sense and specify the preffered rounding,
 *   allowing UFO to choose the best size based on the requested rounding mode. Note that we MAY not respect the rounding mode and the caller must be ready to fulfill a larger request than granted
 *   Repeated calls may continue to grow the fill area, but no call may ever shrink it
 *  The function returns
 *    0                 on success if some change to size was successfully made
 *    ufWarnNoChange    if all arguments were acceptable but no change was made to the range. This can happen for any reason, even spuriously, but a caller should not expect a different result with repeated calls
 *    ufErrShrinksRange if the caller attemptwed to reduce the resolve size and in this case no change is made to the size of the resolve range
 *    ufBadArgs         if rounding mode was not understood
 *
 */
typedef int (*ufPopulateCallout)(*ufPopulateCalloutMsg);

#define ufErrAlreadyResolved 1
#define ufErrOutOfBounds     2
#define ufErrShrinksRange    3
#define ufErrNoMem           4
#define ufWarnNoChange       5
#define ufBadArgs            6

#define ufRoundUp   1
#define ufRoundDown 2

/**
 * @arg start index (inclusive) in the value array
 * @arg end index (exclusive)
 * @arg calback to change the semantics of memory population. See
 * @arg user defined configuration object
 * @arg target memory area to populate
 *
 * All requested memory must be populated when the function returns
 * target  [0     ... n*sdizeof(value)]
 * indexes [start ... end]
 * n = end - start
 * Zero offset in the target is where the startValueidx should go
 */
typedef int (*ufPopulateRange)(uint64_t startValueIdx, uint64_t endValueIdx, ufPopulateCallout callout, ufUserData userData, void* target);


/* Object */

typedef any_t ufObjectConfig_t;
typedef any_t ufObject_t;

ufObjectConfig_t _makeObjectConfig(uint64_t ct, uint32_t stride, int32_t minLoadCt);
//TODO: document
#define makeObjectConfig(type, ct, minLoadCt) \
  _makeObjectConfig(ct, _stride(type), minLoadCt)

void ufSetPopulateFunction(ufObjectConfig_t config, ufPopulateRange populateF);
void ufSetUserConfig(ufObjectConfig_t config, ufUserData userData);

/*
 * returns 0 on success
 * allocates a new UFO and writes a pointer to it in object_p
 * The object config may be resued once this returns
 */
int ufCreateObject(ufInstance_t instance, ufObjectConfig_t objectConfig, ufObject_t* object_p);

/*
 * returns 0 on success
 * Destroys the object and all resources associated with it
 * TODO: What guarntees do we want or need upon this returning?
 */
int ufDestroyObject(ufObject_t object_p);

/*
 * Get the pointer to the R header section of the object
 */
void* ufGetHeaderPointer(ufObject_t object);

/*
 * Get the pointer to the value section of the object
 */
void* ufGetValuePointer(ufObject_t object);


