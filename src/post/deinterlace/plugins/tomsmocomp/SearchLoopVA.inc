// -*- c++ -*-

// Searches the center vertical line above center and below, in both the old 
// and new fields, but takes averages.  These are even pixel addresses.
MERGE4PIXavg (MEMREF3("di","cx","2"), MEMREF1("si"))  // down, up
MERGE4PIXavg (MEMREF1("di"), MEMREF3("si","cx","2"))  // up, down
