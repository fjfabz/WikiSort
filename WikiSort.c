#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <limits.h>

// various #defines for the C code
#ifndef true
	#define true 1
#endif

#define WikiItem(array, index) (array)[index]
#define WikiVar(variable_name, variable_value, more...) WikiTypeOf(variable_value, ##more) variable_name = variable_value, ##more
#define WikiTypeOf(var...) __typeof__(var)
#define WikiSizeOf(var...) sizeof(var)
#define WikiMemCopy memcpy
#define WikiMemMove memmove
#define WikiSqrt(x) sqrt(x)
#define WikiMax(x, y) ({ WikiVar(x1, x); WikiVar(y1, y); (x1 > y1) ? x1 : y1; })
#define WikiMin(x, y) ({ WikiVar(x1, x); WikiVar(y1, y); (x1 < y1) ? x1 : y1; })

// if your language does not support bitwise operations for some reason, you can use (floor(value/2) * 2 == value)
#define WikiIsEven(value) ((value & 0x1) == 0x0)

// 63 -> 32, 64 -> 64, etc.
long WikiFloorPowerOfTwo(long x) {
	x |= (x >> 1); x |= (x >> 2); x |= (x >> 4);
	x |= (x >> 8); x |= (x >> 16); x |= (x >> 32);
	x -= (x >> 1) & 0x5555555555555555;
	x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;
	x += x >> 8; x += x >> 16; x += x >> 32;
	x &= 0x7F; return (x == 0) ? 0 : (1 << (x - 1));
}

// structure to test stable sorting (index will contain its original index in the array, to make sure it doesn't switch places with other items)
typedef struct { int value, index; } WikiTest;
int WikiCompare(WikiTest item1, WikiTest item2) {
	if (item1.value < item2.value) return -1;
	if (item1.value > item2.value) return 1;
	return 0;
}
typedef int (*WikiComparison)(WikiTest, WikiTest);

// structure to represent ranges within the array
#define WikiMakeRange(/* long */ start, length) ((WikiRange){(long)(start), length})
#define WikiRangeBetween(/* long */ start, end) ({ long WikiRangeStart = (long)(start); WikiMakeRange(WikiRangeStart, (end) - WikiRangeStart); })
#define WikiZeroRange() WikiMakeRange(0, 0)
typedef struct { long start, length; } WikiRange;

// toolbox functions used by the sorter

// swap value1 and value2
#define WikiSwap(value1, value2) ({ \
	WikiVar(WikiSwapValue1, &(value1)); \
	WikiVar(WikiSwapValue2, &(value2)); \
	WikiVar(WikiSwapValue, *WikiSwapValue1); \
	*WikiSwapValue1 = *WikiSwapValue2; \
	*WikiSwapValue2 = WikiSwapValue; \
})

// reverse a range within the array
#define WikiReverse(array_instance, range) ({ \
	WikiVar(WikiReverse_array, array_instance); WikiRange WikiReverse_range = range; \
	long WikiReverse_index; \
	for (WikiReverse_index = WikiReverse_range.length/2 - 1; WikiReverse_index >= 0; WikiReverse_index--) \
	WikiSwap(WikiItem(WikiReverse_array, WikiReverse_range.start + WikiReverse_index), WikiItem(WikiReverse_array, WikiReverse_range.start + WikiReverse_range.length - WikiReverse_index - 1)); \
})

// swap a sequence of values in an array
#define WikiBlockSwap(array, start1, start2, block_size) ({ \
	WikiVar(WikiSwap_array, array); WikiVar(WikiSwap_start1, start1); WikiVar(WikiSwap_start2, start2); WikiVar(WikiSwap_size, block_size); \
	if (WikiSwap_size <= cache_size) { \
		WikiMemCopy(&cache[0], &WikiItem(WikiSwap_array, WikiSwap_start1), WikiSwap_size * WikiSizeOf(WikiItem(WikiSwap_array, 0))); \
		WikiMemMove(&WikiItem(WikiSwap_array, WikiSwap_start1), &WikiItem(WikiSwap_array, WikiSwap_start2), WikiSwap_size * WikiSizeOf(WikiItem(WikiSwap_array, 0))); \
		WikiMemCopy(&WikiItem(WikiSwap_array, WikiSwap_start2), &cache[0], WikiSwap_size * WikiSizeOf(WikiItem(WikiSwap_array, 0))); \
	} else { \
		WikiVar(WikiSwap_array, array); WikiVar(WikiSwap_start1, start1); WikiVar(WikiSwap_start2, start2); WikiVar(WikiSwap_size, block_size); \
		long WikiSwap_index; \
		for (WikiSwap_index = 0; WikiSwap_index < WikiSwap_size; WikiSwap_index++) WikiSwap(WikiItem(WikiSwap_array, WikiSwap_start1 + WikiSwap_index), WikiItem(WikiSwap_array, WikiSwap_start2 + WikiSwap_index)); \
	} \
})

// rotate the values in an array ([0 1 2 3] becomes [3 0 1 2] if we rotate by -1)
#define WikiRotate(array, amount, range) ({ \
	WikiVar(WikiRotate_array, array); long WikiRotate_amount = amount; WikiRange WikiRotate_range = range; \
	if (WikiRotate_range.length != 0 && WikiRotate_amount != 0) { \
		if (WikiRotate_amount < 0 && WikiRotate_amount >= -cache_size) { \
			/* copy the right side of the array to the cache, memmove the rest of it, and copy the right side back to the left side */ \
			WikiRotate_amount = (-WikiRotate_amount) % WikiRotate_range.length; \
			long WikiRotate_size = WikiRotate_range.length - WikiRotate_amount; \
			WikiMemCopy(&cache[0], &WikiItem(WikiRotate_array, WikiRotate_range.start + WikiRotate_size), WikiRotate_amount * WikiSizeOf(WikiItem(WikiRotate_array, 0))); \
			WikiMemMove(&WikiItem(WikiRotate_array, WikiRotate_range.start + WikiRotate_amount), &WikiItem(WikiRotate_array, WikiRotate_range.start), WikiRotate_size * WikiSizeOf(WikiItem(WikiRotate_array, 0))); \
			WikiMemCopy(&WikiItem(WikiRotate_array, WikiRotate_range.start), &cache[0], WikiRotate_amount * WikiSizeOf(WikiItem(WikiRotate_array, 0))); \
		} else if (WikiRotate_amount > 0 && WikiRotate_amount <= cache_size) { \
			WikiRotate_amount = WikiRotate_amount % WikiRotate_range.length; \
			long WikiRotate_size = WikiRotate_range.length - WikiRotate_amount; \
			WikiMemCopy(&cache[0], &WikiItem(WikiRotate_array, WikiRotate_range.start), WikiRotate_amount * WikiSizeOf(WikiItem(WikiRotate_array, 0))); \
			WikiMemMove(&WikiItem(WikiRotate_array, WikiRotate_range.start), &WikiItem(WikiRotate_array, WikiRotate_range.start + WikiRotate_amount), WikiRotate_size * WikiSizeOf(WikiItem(WikiRotate_array, 0))); \
			WikiMemCopy(&WikiItem(WikiRotate_array, WikiRotate_range.start + WikiRotate_size), &cache[0], WikiRotate_amount * WikiSizeOf(WikiItem(WikiRotate_array, 0))); \
		} else { \
			WikiRange WikiRotate_range1, WikiRotate_range2; \
			if (WikiRotate_amount < 0) { \
				WikiRotate_amount = (-WikiRotate_amount) % WikiRotate_range.length; \
				WikiRotate_range1 = WikiMakeRange(WikiRotate_range.start + WikiRotate_range.length - WikiRotate_amount, WikiRotate_amount); \
				WikiRotate_range2 = WikiMakeRange(WikiRotate_range.start, WikiRotate_range.length - WikiRotate_amount); \
			} else { \
				WikiRotate_amount = WikiRotate_amount % WikiRotate_range.length; \
				WikiRotate_range1 = WikiMakeRange(WikiRotate_range.start, WikiRotate_amount); \
				WikiRotate_range2 = WikiMakeRange(WikiRotate_range.start + WikiRotate_amount, WikiRotate_range.length - WikiRotate_amount); \
			} \
			WikiReverse(WikiRotate_array, WikiRotate_range1); \
			WikiReverse(WikiRotate_array, WikiRotate_range2); \
			WikiReverse(WikiRotate_array, WikiRotate_range); \
		} \
	} \
})

// merge operation which uses an internal or external buffer
#define WikiMerge(array, buffer, A, B, compare) ({ \
	WikiVar(WikiMerge_array, array); WikiVar(WikiMerge_buffer, buffer); WikiVar(WikiMerge_A, A); WikiVar(WikiMerge_B, B); \
	if (compare(WikiItem(WikiMerge_array, WikiMerge_A.start + WikiMerge_A.length - 1), WikiItem(WikiMerge_array, WikiMerge_B.start)) > 0) { \
		long A_count = 0, B_count = 0, insert = 0; \
		if (WikiMerge_A.length <= cache_size) { \
			WikiMemCopy(&cache[0], &WikiItem(WikiMerge_array, WikiMerge_A.start), WikiMerge_A.length * WikiSizeOf(WikiItem(WikiMerge_array, 0))); \
			while (A_count < WikiMerge_A.length && B_count < WikiMerge_B.length) \
				WikiItem(WikiMerge_array, WikiMerge_A.start + insert++) = (compare(cache[A_count], WikiItem(WikiMerge_array, WikiMerge_B.start + B_count)) <= 0) ? cache[A_count++] : WikiItem(WikiMerge_array, WikiMerge_B.start + B_count++); \
			WikiMemCopy(&WikiItem(WikiMerge_array, WikiMerge_A.start + insert), &cache[A_count], (WikiMerge_A.length - A_count) * WikiSizeOf(WikiItem(WikiMerge_array, 0))); \
		} else { \
			WikiBlockSwap(WikiMerge_array, WikiMerge_buffer.start, WikiMerge_A.start, WikiMerge_A.length); \
			while (A_count < A.length && B_count < B.length) { \
				if (compare(WikiItem(WikiMerge_array, WikiMerge_buffer.start + A_count), WikiItem(WikiMerge_array, WikiMerge_B.start + B_count)) <= 0) { \
					WikiSwap(WikiItem(WikiMerge_array, WikiMerge_A.start + insert++), WikiItem(WikiMerge_array, WikiMerge_buffer.start + A_count++)); \
				} else WikiSwap(WikiItem(WikiMerge_array, WikiMerge_A.start + insert++), WikiItem(WikiMerge_array, WikiMerge_B.start + B_count++)); \
			} \
			WikiBlockSwap(WikiMerge_array, WikiMerge_buffer.start + A_count, WikiMerge_A.start + insert, WikiMerge_A.length - A_count); \
		} \
	} \
})

// find the first value within the range that is equal to the value at index
long WikiBinaryInsertFirst(WikiTest array[], long index, WikiRange range, WikiComparison compare) {
	long min1 = range.start, max1 = range.start + range.length - 1;
	while (min1 < max1) { long mid1 = min1 + (max1 - min1)/2; if (compare(WikiItem(array, mid1), WikiItem(array, index)) < 0) min1 = mid1 + 1; else max1 = mid1; }
	if (min1 == range.start + range.length - 1 && compare(WikiItem(array, min1), WikiItem(array, index)) < 0) min1++;
	return min1;
}

// find the last value within the range that is equal to the value at index. the result is 1 more than the last index
long WikiBinaryInsertLast(WikiTest array[], long index, WikiRange range, WikiComparison compare) {
	long min1 = range.start, max1 = range.start + range.length - 1;
	while (min1 < max1) { long mid1 = min1 + (max1 - min1)/2; if (compare(WikiItem(array, mid1), WikiItem(array, index)) <= 0) min1 = mid1 + 1; else max1 = mid1; }
	if (min1 == range.start + range.length - 1 && compare(WikiItem(array, min1), WikiItem(array, index)) <= 0) min1++;
	return min1;
}

// n^2 sorting algorithm, used to sort tiny chunks of the full array
void WikiInsertionSort(WikiTest array[], WikiRange range, WikiComparison compare) {
	long i;
	for (i = range.start + 1; i < range.start + range.length; i++) {
		WikiTest temp = WikiItem(array, i); long j = i;
		while (j > range.start && compare(WikiItem(array, j - 1), temp) > 0) { WikiItem(array, j) = WikiItem(array, j - 1); j--; }
		WikiItem(array, j) = temp;
	}
}

// bottom-up merge sort combined with an in-place merge algorithm for O(1) memory use
void WikiSort(WikiTest array[], const long array_count, WikiComparison compare) {
	// the various toolbox functions are optimized to take advantage of this cache, so tweak it as desired
	// generally this cache is suitable for arrays of up to size (cache_size^2)
	const long cache_size = 1024;
	WikiTest cache[cache_size];
	
	if (array_count < 32) {
		// insertion sort the entire array, since there are fewer than 32 items
		WikiInsertionSort(array, WikiRangeBetween(0, array_count), compare);
		return;
	}
	
	// calculate how to scale the index value to the range within the array
	long power_of_two = WikiFloorPowerOfTwo(array_count);
	double scale = array_count/(double)power_of_two; // 1.0 <= scale < 2.0
	
	long index, iteration;
	for (index = 0; index < power_of_two; index += 32) {
		// insertion sort from start to mid and mid to end
		long start = index * scale;
		long mid = (index + 16) * scale;
		long end = (index + 32) * scale;
		
		WikiInsertionSort(array, WikiRangeBetween(start, mid), compare);
		WikiInsertionSort(array, WikiRangeBetween(mid, end), compare);
		
		// here's where the fake recursion is handled
		// it's a bottom-up merge sort, but multiplying by scale is more efficient than using min(end, array_count)
		long merge = index, length = 16;
		for (iteration = index/16 + 2; WikiIsEven(iteration); iteration /= 2) {
			start = merge * scale;
			mid = (merge + length) * scale;
			end = (merge + length + length) * scale;
			
			if (compare(WikiItem(array, start), WikiItem(array, end - 1)) > 0) {
				// the two ranges are in reverse order, so a simple rotation should fix it
				WikiRotate(array, mid - start, WikiRangeBetween(start, end));
			} else if (compare(WikiItem(array, mid - 1), WikiItem(array, mid)) > 0) {
				// these two ranges weren't already in order, so we'll need to merge them!
				WikiRange A = WikiRangeBetween(start, mid), B = WikiRangeBetween(mid, end);
				
				// if A fits into the cache, perform a simple merge; otherwise perform a trickier in-place merge
				if (A.length <= cache_size) {
					WikiMemCopy(&cache[0], &WikiItem(array, A.start), A.length * WikiSizeOf(WikiItem(array, 0)));
					long A_count = 0, B_count = 0, insert = 0;
					while (A_count < A.length && B_count < B.length) WikiItem(array, A.start + insert++) = (compare(cache[A_count], WikiItem(array, B.start + B_count)) <= 0) ? cache[A_count++] : WikiItem(array, B.start + B_count++);
					WikiMemCopy(&WikiItem(array, A.start + insert), &cache[A_count], (A.length - A_count) * WikiSizeOf(WikiItem(array, 0)));
				} else {
					// try to fill up two buffers with unique values in ascending order
					WikiRange bufferA, bufferB, buffer1, buffer2; long block_size = WikiMax(WikiSqrt(A.length), 2), buffer_size = A.length/block_size;
					for (buffer1.start = A.start + 1, buffer1.length = 1; buffer1.start < A.start + A.length && buffer1.length < buffer_size; buffer1.start++)
						if (compare(WikiItem(array, buffer1.start - 1), WikiItem(array, buffer1.start)) != 0) buffer1.length++;
					for (buffer2.start = buffer1.start, buffer2.length = 0; buffer2.start < A.start + A.length && buffer2.length < buffer_size; buffer2.start++)
						if (compare(WikiItem(array, buffer2.start - 1), WikiItem(array, buffer2.start)) != 0) buffer2.length++;
					
					if (buffer2.length == buffer_size) {
						// we found enough values for both buffers in A
						bufferA = WikiMakeRange(buffer2.start, buffer_size * 2);
						bufferB = WikiMakeRange(B.start + B.length, 0);
						buffer1 = WikiMakeRange(A.start, buffer_size);
						buffer2 = WikiMakeRange(A.start + buffer_size, buffer_size);
					} else if (buffer1.length == buffer_size) {
						// we found enough values for one buffer in A, so we'll need to find one buffer in B
						bufferA = WikiMakeRange(buffer1.start, buffer_size);
						buffer1 = WikiMakeRange(A.start, buffer_size);
						
						for (buffer2.start = B.start + B.length - 1, buffer2.length = 1; buffer2.start >= B.start && buffer2.length < buffer_size; buffer2.start--)
							if (buffer2.start == B.start || compare(WikiItem(array, buffer2.start - 1), WikiItem(array, buffer2.start)) != 0) buffer2.length++;
						if (buffer2.length == buffer_size) {
							bufferB = WikiMakeRange(buffer2.start, buffer_size);
							buffer2 = WikiMakeRange(B.start + B.length - buffer_size, buffer_size);
						}
					} else {
						// we were unable to find a single buffer in A, so we'll need to find two buffers in B
						for (buffer1.start = B.start + B.length - 1, buffer1.length = 1; buffer1.start >= B.start && buffer1.length < buffer_size; buffer1.start--)
							if (buffer1.start == B.start || compare(WikiItem(array, buffer1.start - 1), WikiItem(array, buffer1.start)) != 0) buffer1.length++;
						for (buffer2.start = buffer1.start - 1, buffer2.length = 1; buffer2.start >= B.start && buffer2.length < buffer_size; buffer2.start--)
							if (buffer2.start == B.start || compare(WikiItem(array, buffer2.start - 1), WikiItem(array, buffer2.start)) != 0) buffer2.length++;
						
						if (buffer2.length == buffer_size) {
							bufferA = WikiMakeRange(A.start, 0);
							bufferB = WikiMakeRange(buffer2.start, buffer_size * 2);
							buffer1 = WikiMakeRange(B.start + B.length - buffer_size, buffer_size);
							buffer2 = WikiMakeRange(buffer1.start - buffer_size, buffer_size);
						}
					}
					
					if (buffer2.length < buffer_size) {
						// we failed to fill both buffers with unique values, which implies we're merging two subarrays with a lot of the same values repeated
						// we can use this knowledge to write a merge operation that is optimized for arrays of repeating values
						
						// this is the rotation-based variant of the Hwang-Lin merge algorithm
						while (true) {
							if (A.length <= B.length) {
								if (A.length <= 0) break;
								long block_size = WikiMax(WikiFloorPowerOfTwo((long)(B.length/(double)A.length)), 1);
								
								// merge A[first] into B
								long index = B.start + block_size;
								while (index < B.start + B.length && compare(WikiItem(array, index), WikiItem(array, A.start)) < 0) index += block_size;
								
								// binary search to find the first index where B[index - 1] < A[first] <= B[index]
								long min1 = WikiBinaryInsertFirst(array, A.start, WikiRangeBetween(index - block_size, WikiMin(index, B.start + B.length)), compare);
								
								// rotate [A B1] B2 to [B1 A] B2 and recalculate A and B
								WikiRotate(array, A.length, WikiRangeBetween(A.start, min1));
								A.length--; A.start = min1 - A.length;
								B = WikiRangeBetween(min1, B.start + B.length);
							} else {
								if (B.length <= 0) break;
								long block_size = WikiMax(WikiFloorPowerOfTwo((long)(A.length/(double)B.length)), 1);
								
								// merge B[last] into A
								long index = B.start - block_size;
								while (index >= A.start && compare(WikiItem(array, index), WikiItem(array, B.start + B.length - 1)) >= 0) index -= block_size;
								
								// binary search to find the last index where A[index - 1] <= B[last] < A[index]
								long min1 = WikiBinaryInsertLast(array, B.start + B.length - 1, WikiRangeBetween(WikiMax(index, A.start), index + block_size), compare);
								
								// rotate A1 [A2 B] to A1 [B A2] and recalculate A and B
								WikiRotate(array, -B.length, WikiRangeBetween(min1, B.start + B.length));
								A = WikiRangeBetween(A.start, min1);
								B = WikiMakeRange(min1, B.length - 1);
							}
						}
						
						return;
					}
					
					// move the unique values to the start of A if needed
					long index, count;
					for (index = bufferA.start - 2, count = 1; count < bufferA.length; index--) {
						if (index == A.start || compare(WikiItem(array, index - 1), WikiItem(array, index)) != 0) {
							WikiRotate(array, -count, WikiRangeBetween(index + 1, bufferA.start));
							count++; bufferA.start = index + count;
						}
					}
					bufferA.start = A.start;
					
					// move the unique values to the end of B if needed
					for (index = bufferB.start + 1, count = 0; count < bufferB.length; index++) {
						if (index == B.start + B.length || compare(WikiItem(array, index - 1), WikiItem(array, index)) != 0) {
							WikiRotate(array, count + 1, WikiRangeBetween(bufferB.start, index));
							count++; bufferB.start = index - count;
						}
					}
					bufferB.start = B.start + B.length - bufferB.length;
					
					// break the remainder of A into blocks, which we'll call w. w0 is the uneven-sized first w block
					WikiRange w = WikiRangeBetween(bufferA.start + bufferA.length, A.start + A.length);
					WikiRange w0 = WikiMakeRange(bufferA.start + bufferA.length, w.length % block_size);
					
					// swap the last value of each w block with the value in buffer1
					long w_index;
					for (index = 0, w_index = w0.start + w0.length + block_size - 1; w_index < w.start + w.length; index++, w_index += block_size)
						WikiSwap(WikiItem(array, buffer1.start + index), WikiItem(array, w_index));
					
					WikiRange last_w = w0, last_v = WikiZeroRange(), v = WikiMakeRange(B.start, WikiMin(block_size, B.length - bufferB.length));
					w.start += w0.length; w.length -= w0.length;
					w_index = 0;
					long w_min = w.start;
					while (w.length > 0) {
						// if there's a previous v block and the first value of the minimum w block is <= the last value of the previous v block
						if ((last_v.length > 0 && compare(WikiItem(array, w_min), WikiItem(array, last_v.start + last_v.length - 1)) <= 0) || v.length == 0) {
							// figure out where to split the previous v block, and rotate it at the split
							long v_split = WikiBinaryInsertFirst(array, w_min, last_v, compare);
							long v_remaining = last_v.start + last_v.length - v_split;
							
							// swap the minimum w block to the beginning of the rolling w blocks
							WikiBlockSwap(array, w.start, w_min, block_size);
							
							// we need to swap the last item of the previous w block back with its original value, which is stored in buffer1
							// since the w0 block did not have its value swapped out, we need to make sure the previous w block is not unevenly sized
							WikiSwap(WikiItem(array, w.start + block_size - 1), WikiItem(array, buffer1.start + w_index++));
							
							// now we need to split the previous v block at v_split and insert the minimum w block in-between the two parts, using a rotation
							WikiRotate(array, v_remaining, WikiRangeBetween(v_split, w.start + block_size));
							
							// locally merge the previous w block with the v blocks that follow it, using the buffer as swap space
							WikiMerge(array, buffer2, last_w, WikiRangeBetween(last_w.start + last_w.length, v_split), compare);
							
							// now we need to update the ranges and stuff
							last_w = WikiMakeRange(w.start - v_remaining, block_size);
							last_v = WikiMakeRange(last_w.start + last_w.length, v_remaining);
							w.start += block_size; w.length -= block_size;
							
							// search the last value of the remaining w blocks to find the new minimum w block (that's why we wrote unique values to them!)
							w_min = w.start + block_size - 1;
							long w_find;
							for (w_find = w_min + block_size; w_find < w.start + w.length; w_find += block_size)
								if (compare(WikiItem(array, w_find), WikiItem(array, w_min)) < 0) w_min = w_find;
							w_min -= (block_size - 1);
						} else if (v.length < block_size) {
							// move the last v block, which is unevenly sized, to before the remaining w blocks, by using a rotation
							WikiRotate(array, -v.length, WikiRangeBetween(w.start, v.start + v.length));
							last_v = WikiMakeRange(w.start, v.length);
							w.start += v.length; w_min += v.length;
							v.length = 0;
						} else {
							// roll the leftmost w block to the end by swapping it with the next v block
							WikiBlockSwap(array, w.start, v.start, block_size);
							last_v = WikiMakeRange(w.start, block_size);
							if (w_min == w.start) w_min = w.start + w.length;
							w.start += block_size;
							v.start += block_size;
							if (v.start + v.length > bufferB.start) v.length = bufferB.start - v.start;
						}
					}
					
					WikiMerge(array, buffer2, last_w, WikiRangeBetween(last_w.start + last_w.length, B.start + B.length - bufferB.length), compare);
					
					// when we're finished with this step we should have b1 b2 left over, where one of the buffers is all jumbled up
					// insertion sort the jumbled up buffer, then redistribute them back into the array using the opposite process used for creating the buffer
					WikiInsertionSort(array, buffer2, compare);
					
					// redistribute bufferA back into the array
					for (index = bufferA.start + bufferA.length; bufferA.length > 0; index++) {
						if (index == bufferB.start || compare(WikiItem(array, index), WikiItem(array, bufferA.start)) >= 0) {
							long amount = index - (bufferA.start + bufferA.length);
							WikiRotate(array, -amount, WikiRangeBetween(bufferA.start, index));
							bufferA.start += amount + 1; bufferA.length--; index--;
						}
					}
					
					// redistribute bufferB back into the array
					for (index = bufferB.start; bufferB.length > 0; index--) {
						if (index == A.start || compare(WikiItem(array, index - 1), WikiItem(array, bufferB.start + bufferB.length - 1)) <= 0) {
							long amount = bufferB.start - index;
							WikiRotate(array, amount, WikiRangeBetween(index, bufferB.start + bufferB.length));
							bufferB.start -= amount; bufferB.length--; index++;
						}
					}
				}
			}
			
			// the merges get twice as large after each iteration, until eventually we merge the entire array
			length += length; merge -= length;
		}
	}
}

// standard merge sort
void MergeSortR(WikiTest array[], WikiRange range, WikiComparison compare, WikiTest buffer[]) {
	if (range.length < 32) {
		WikiInsertionSort(array, range, compare);
		return;
	}
	
	long mid = range.start + range.length/2;
	WikiRange A = WikiRangeBetween(range.start, mid), B = WikiRangeBetween(mid, range.start + range.length);
	
	MergeSortR(array, A, compare, buffer);
	MergeSortR(array, B, compare, buffer);
	
	// standard merge operation here (only A is copied to the buffer)
	WikiMemCopy(&WikiItem(buffer, 0), &WikiItem(array, A.start), A.length * WikiSizeOf(WikiItem(array, 0)));
	long A_count = 0, B_count = 0, insert = 0;
	while (A_count < A.length && B_count < B.length) {
		if (compare(WikiItem(buffer, A_count), WikiItem(array, A.start + A.length + B_count)) <= 0) {
			WikiItem(array, A.start + insert++) = WikiItem(buffer, A_count++);
		} else {
			WikiItem(array, A.start + insert++) = WikiItem(array, A.start + A.length + B_count++);
		}
	}
	WikiMemCopy(&WikiItem(array, A.start + insert), &WikiItem(buffer, A_count), (A.length - A_count) * WikiSizeOf(WikiItem(array, 0)));
}

void MergeSort(WikiTest array[], const long array_count, WikiComparison compare) {
	WikiTest *buffer = malloc(array_count * WikiSizeOf(WikiItem(array, 0)));
	MergeSortR(array, WikiMakeRange(0, array_count), compare, buffer);
	free(buffer);
}

int main(int argc, char argv[]) {
	long total, index;
	const long max_size = 3000000;
	WikiTest *array1 = malloc(max_size * WikiSizeOf(WikiTest)), *array2 = malloc(max_size * WikiSizeOf(WikiTest));
	WikiComparison compare = WikiCompare;
	
	srand(/*time(NULL)*/ 10141985);
	
	for (total = 0; total < max_size; total += 2048 * 16) {
		for (index = 0; index < total; index++) {
			WikiTest item; item.index = index;
			
			// uncomment the type of data you want to fill the arrays with
			item.value = rand();
			//item.value = total - index + rand() * 1.0/INT_MAX * 5 - 2.5;
			//item.value = index + rand() * 1.0/INT_MAX * 5 - 2.5;
			//item.value = index;
			//item.value = total - index;
			//item.value = 1000;
			//item.value = (rand() * 1.0/INT_MAX <= 0.9) ? index : (index - 2);
			
			WikiItem(array1, index) = WikiItem(array2, index) = item;
		}
		
		double time1 = clock();
		WikiSort(array1, total, compare);
		time1 = (clock() - time1) * 1.0/CLOCKS_PER_SEC;
		
		double time2 = clock();
		MergeSort(array2, total, compare);
		time2 = (clock() - time2) * 1.0/CLOCKS_PER_SEC;
		
		printf("[%ld] wiki: %f, merge: %f (%f%%)\n", total, time1, time2, time2/time1 * 100.0);
		
		// make sure the arrays are sorted correctly, and that the results were stable
		assert(compare(WikiItem(array1, 0), WikiItem(array2, 0)) == 0);
		for (index = 1; index < total; index++) {
			assert(compare(WikiItem(array1, index), WikiItem(array2, index)) == 0);
			assert(compare(WikiItem(array1, index), WikiItem(array1, index - 1)) > 0 ||
				   (compare(WikiItem(array1, index), WikiItem(array1, index - 1)) == 0 && WikiItem(array1, index).index > WikiItem(array1, index - 1).index));
		}
	}
	
	free(array1); free(array2);
	return 0;
}