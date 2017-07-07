// Searches 1 pixel to the left and right, in both the old
//  and new fields, but takes v-half pel averages. These are odd
// pixel addresses. Any chroma match will not be used. (YUY2)
MERGE4PIXavgH ("-2"MEMREF1("di"), "-2"MEMREF2("di","cx"), "2"MEMREF2("si","cx"), "2"MEMREF3("si","cx","2"))  // up left, down right
MERGE4PIXavgH ("2"MEMREF1("di"), "2"MEMREF2("di","cx"), "-2"MEMREF2("si","cx"), "-2"MEMREF3("si","cx","2"))  // up right, down left
MERGE4PIXavgH ("-2"MEMREF3("di","cx","2"), "-2"MEMREF2("di","cx"), "2"MEMREF2("si","cx"), "2"MEMREF1("si"))  // down left, up right
MERGE4PIXavgH ("2"MEMREF3("di","cx","2"), "2"MEMREF2("di","cx"), "-2"MEMREF2("si","cx"), "-2"MEMREF1("si"))  // down right, up left
