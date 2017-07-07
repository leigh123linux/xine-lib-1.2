// Searches 1 pixel to the left and right, in both the old
// and new fields, but takes averages. These are odd
// pixel addresses. Any chroma match will not be used. (YUY2)
MERGE4PIXavg ("-2"MEMREF2("di","cx"), "2"MEMREF2("si","cx"))  // left, right
MERGE4PIXavg ("2"MEMREF2("di","cx"), "-2"MEMREF2("si","cx"))  // right, left
