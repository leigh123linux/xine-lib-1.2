// Searches 1 pixel to the left and right, in both the old
//  and new fields, but takes vertical averages. These are odd
// pixel addresses. Any chroma match will not be used. (YUY2)
MERGE4PIXavgH ("-2"MEMREF2("di","cx"), MEMREF2("di","cx"), MEMREF2("si","cx"), "2"MEMREF2("si","cx"))  // left, right
MERGE4PIXavgH ("2"MEMREF2("di","cx"), MEMREF2("di","cx"), MEMREF2("si","cx"), "-2"MEMREF2("si","cx"))  // right, left
