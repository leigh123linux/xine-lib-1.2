// -*- c++ -*-       

#ifdef IS_SSE2
//sse2 code deleted for now
#else

// Version for non-SSE2

#ifdef SKIP_SEARCH
            "movq    %%mm6, %%mm0\n\t"            // just use the results of our wierd bob
#else


            // JA 9/Dec/2002
            // failed experiment
            // but leave in placeholder for me to play about
#ifdef DONT_USE_STRANGE_BOB
            // Use the best weave if diffs less than 10 as that
            // means the image is still or moving cleanly
            // if there is motion we will clip which will catch anything
            "psubusb "_FOURS", %%mm7\n\t"          // sets bits to zero if weave diff < 4
            "pxor    %%mm0, %%mm0\n\t"
            "pcmpeqb %%mm0, %%mm7\n\t"            // all ff where weave better, else 00
            "pcmpeqb %%mm7, %%mm0\n\t"            // all ff where bob better, else 00
            "pand    %%mm6, %%mm0\n\t"            // use bob for these pixel values
            "pand    %%mm5, %%mm7\n\t"            // use weave for these
            "por     %%mm7, %%mm0\n\t"            // combine both
#else
            // Use the better of bob or weave
            //      pminub  mm4, TENS           // the most we care about
            V_PMINUB ("%%mm4", _TENS, "%%mm0")   // the most we care about
            
            "psubusb %%mm4, %%mm7\n\t"            // foregive that much from weave est?
            "psubusb "_FOURS", %%mm7\n\t"       // bias it a bit toward weave
            "pxor    %%mm0, %%mm0\n\t"
            "pcmpeqb %%mm0, %%mm7\n\t"            // all ff where weave better, else 00
            "pcmpeqb %%mm7, %%mm0\n\t"            // all ff where bob better, else 00
            "pand    %%mm6, %%mm0\n\t"            // use bob for these pixel values
            "pand    %%mm5, %%mm7\n\t"            // use weave for these
            "por     %%mm7, %%mm0\n\t"            // combine both
#endif
            
            
                //      pminub  mm0, Max_Vals       // but clip to catch the stray error
//                V_PMINUB ("%%mm0", _Max_Vals, "%%mm1") // but clip to catch the stray error
                //      pmaxub  mm0, Min_Vals
//                V_PMAXUB ("%%mm0", _Min_Vals)
                
#endif


            MEMREG ("mov", _pDest, "ax")
            MEMREG ("add", _olddx, "ax")

#ifdef USE_VERTICAL_FILTER
            "movq    %%mm0, %%mm1\n\t"
            //      pavgb   mm0, qword ptr["XDX"]
            V_PAVGB ("%%mm0", MEMREF1("dx"), "%%mm2", _ShiftMask)
            //      movntq  qword ptr["XAX"+"_olddx"], mm0
            V_MOVNTQ (MEMREF1("ax"), "%%mm0")
            //      pavgb   mm1, qword ptr["XDX"+"XCX"]
            V_PAVGB ("%%mm1", MEMREF2("dx","cx"), "%%mm2", _ShiftMask)
            MEMREG ("add", _dst_pitchw, "dx")
            //      movntq  qword ptr["XAX"+"_olddx"], mm1
            V_MOVNTQ (MEMREF1("ax"), "%%mm1")
#else
                
            //      movntq  qword ptr["XAX"+"_olddx"], mm0
            V_MOVNTQ (MEMREF1("ax"), "%%mm0")
#endif
                
            CONSTMEM ("add", "8", _olddx)   // bump offset pointer
            MEMREG ("mov", _olddx, "ax")
            MEMREG ("cmp", _Last8, "ax")    // done with line?
            "jb      1b\n\t"         // y
#endif

        : /* no outputs */

        : "m"(pBob),
          "m"(src_pitch2),
          "m"(pDest),
          "m"(dst_pitchw),
          "m"(Last8),
          "m"(pSrc),
          "m"(pSrcP),
          "m"(pBobP),
          "m"(olddx),
          "m"(UVMask),
          "m"(ShiftMask),
          "m"(FOURS),
          "m"(TENS),
          "m"(Max_Vals),
          "m"(Min_Vals),
          "m"(YMask),
          "m"(Max_Mov),
          "m"(ONES),
          "m"(DiffThres)

        :
#if defined(ARCH_X86_64) || defined(ARCH_X86_X32)
          "rax", "rcx", "rdx", "rsi", "rdi",
/* the following clobber list causes trouble for gcc 2.95. it shouldn't be
 * an issue as, afaik, mmx registers map to the existing fp registers.
 */
          "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
#elif defined(ARCH_X86)
          "eax", "ecx", "edx", "esi", "edi",
          "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)",
#endif
          "memory", "cc"
        );

        // adjust for next line
        pSrc  += src_pitch2;
        pSrcP += src_pitch2;
        pDest += dst_pitch2;
        pBob  += src_pitch2;
        pBobP += src_pitch2;
    }
    
    goto end;
