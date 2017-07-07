// -*- c++ -*-

// Searches 3 pixels to the left and right, in both the old
//  and new fields, but takes averages. These are odd
// pixel addresses. Any chroma match will not be used. (YUY2)
MERGE4PIXavg ("-6"MEMREF1("di"), "6"MEMREF3("si","cx","2"))  // up left, down right
MERGE4PIXavg ("6"MEMREF1("di"), "-6"MEMREF3("si","cx","2"))  // up right, down left
MERGE4PIXavg ("-6"MEMREF2("di","cx"), "6"MEMREF2("si","cx")) // left, right
MERGE4PIXavg ("6"MEMREF2("di","cx"), "-6"MEMREF2("si","cx")) // right, left
MERGE4PIXavg ("-6"MEMREF3("di","cx","2"), "6"MEMREF1("si"))  // down left, up right
MERGE4PIXavg ("6"MEMREF3("di","cx","2"), "-6"MEMREF1("si"))  // down right, up left
