// -*- c++ -*-

// Searches the center vertical line above center and below, in both the old 
// and new fields, but takes averages.  These are even pixel addresses.
MERGE4PIXavgH (MEMREF3("di","cx","2"), MEMREF2("di","cx"), MEMREF2("si","cx"), MEMREF1("si"))  // down, up
MERGE4PIXavgH (MEMREF1("di"), MEMREF2("di","cx"), MEMREF2("si","cx"), MEMREF3("si","cx","2"))  // up, down
