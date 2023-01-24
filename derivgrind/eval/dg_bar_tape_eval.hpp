/*! \enum TapefileEvents
 * Event types passed to an optional event handler template argument
 * of Tapefile, to enable performance measurements.
 */
enum TapefileEvent { 
  EvaluateChunkBegin,
  EvaluateChunkEnd
};

template<unsigned long long bufsize, typename loadfun_t, void(*eventhandler)(TapefileEvent)=nullptr>
class Tapefile {
  using ull = unsigned long long;
  ull number_of_blocks; //!< Number of blocks on the tape.
  ull tape_buf[4*bufsize]; //!< Buffers one "chunk", i.e. bufsize-many blocks of the tape.
  loadfun_t loadfun; //!< Tapefile members call loadfun(i,count,tape_buf) to load count-many blocks, starting at index i.

private:
  /*! Implementation of iterate(..), information if forward or backward order is template argument.
   *
   */
  template<typename fun_t, bool forward>
  void iterate_impl(ull begin, ull end, fun_t fun){
    ull number_of_blocks_in_subtape = forward ? (end-begin+1) : (begin-end+1);
    // We divide the number_of_blocks_in_subtape many blocks into number_of_chunks_in_subtape many chunks.
    // These chunks are loaded at once, and then iterated through in the correct direction.
    // Each chunk contains bufsize many blocks, except the last one, which contains 0,...,bufsize-1 many blocks.
    ull number_of_chunks_in_subtape = number_of_blocks_in_subtape / bufsize + 1;
    for(ull chunk=0; chunk<number_of_chunks_in_subtape; chunk++){
      ull chunk_count = (chunk==number_of_chunks_in_subtape-1) ? (number_of_blocks_in_subtape - (number_of_chunks_in_subtape-1)*bufsize) : bufsize;
      ull chunk_begin = forward ? (begin+chunk*bufsize) : (begin-chunk*bufsize-chunk_count+1);
      loadfun(chunk_begin, chunk_count, tape_buf);
      if(eventhandler) eventhandler(EvaluateChunkBegin);
      for(long long block_in_chunk= (forward ? 0 : chunk_count-1);
          forward ? (block_in_chunk<chunk_count) : (block_in_chunk>=0); 
          block_in_chunk += (forward ? 1 : -1)) {
        ull index1 = tape_buf[4*block_in_chunk];
        ull index2 = tape_buf[4*block_in_chunk+1];
        double diff1 = *reinterpret_cast<double*>(&tape_buf[4*block_in_chunk+2]);
        double diff2 = *reinterpret_cast<double*>(&tape_buf[4*block_in_chunk+3]);
        ull index = forward ? (begin+chunk*bufsize+block_in_chunk) : (begin-chunk*bufsize-chunk_count+block_in_chunk+1);
        fun(index, index1, index2, diff1, diff2);
      }
      if(eventhandler) eventhandler(EvaluateChunkEnd);
    }
  }

public:

  Tapefile(loadfun_t& loadfun, ull number_of_blocks) : loadfun(loadfun), number_of_blocks(number_of_blocks) {}

  /*! Iterate over sequence of consecutive blocks on the tape, either in forward or backward order.
   *
   * \param begin Index of first block included in the iteration.
   * \param end Index of last block included in the iteration.
   * \param fun Function fun(index, index1, index2, diff1, diff2) called for each block.
   */
  template<typename fun_t>
  void iterate(ull begin, ull end, fun_t fun){
    if(end >= begin)
      iterate_impl<fun_t,true>(begin,end,fun);
    else
      iterate_impl<fun_t,false>(begin,end,fun);
  }

  /*! Reverse evaluation of the tape.
   *
   * \param adjointvec Adjoint vector with the signature of a double[number_of_blocks]. Must be a initialized with zeros and output adjoints before calling this function.
   */
  template<typename adjointvec_t>
  void evaluateBackward(adjointvec_t& adjointvec){
    iterate(number_of_blocks-1, 0, [&adjointvec](ull index, ull index1, ull index2, double diff1, double diff2){
      if(adjointvec[index]!=0) {
        if(index1!=0 && index1 < 0x8000000000000000) adjointvec[index1] += adjointvec[index] * diff1;
        if(index2!=0 && index2 < 0x8000000000000000) adjointvec[index2] += adjointvec[index] * diff2;
      }
    });
  }

  /*! Forward evaluation of the tape.
   *
   * \param adjointvec Adjoint vector with the signature of a double[number_of_blocks]. Must be a initialized with zeros and input dot values before calling this function.
   */
  template<typename adjointvec_t>
  void evaluateForward(adjointvec_t& adjointvec){
    iterate(0, number_of_blocks-1, [&adjointvec](ull index, ull index1, ull index2, double diff1, double diff2){
      if(index1!=0 && index1 < 0x8000000000000000 && adjointvec[index1]!=0){
        adjointvec[index] += adjointvec[index1] * diff1;
      }
      if(index2!=0 && index2 < 0x8000000000000000 && adjointvec[index2]!=0){
        adjointvec[index] += adjointvec[index2] * diff2;
      }
    });
  }

  /*! Get tape statistics.
   *
   * \param nZero Number of blocks with two times a zero index, i.e., input variables plus one.
   * \param nOne Number of blocks with one zero and one non-zero index.
   * \param nTwo Number of blocks with two non-zero indices.
   */
  void stats(ull& nZero, ull& nOne, ull& nTwo){
    nZero = nOne = nTwo = 0;
    iterate(0,number_of_blocks-1, [&nZero,&nOne,&nTwo](ull index, ull index1, ull index2, double diff1, double diff2){
      if(index1==0 && index2==0){
        nZero++;
      } else if(index1!=0 && index2!=0){
        nTwo++;
      } else {
        nOne++;
      }
    });
  }

  /*! Scan the tape for variables that influence the result, but were not recognized as the result of a floating-point operation.
   * 
   * When Derivgrind is running with --typegrind=yes, it emits an index larger or equal 0x80..0 for the result of all operations that it does not recognize as real arithmetic.
   * The existence of such operations is not necessarily problematic. Only if such a result is used to compute the output to be differentiated, Derivgrind might overlook
   * some real-arithmetic dependency. This function proceeds like a tape evaluation to find out which indices can influence the output, and notifies the caller with a callback
   *  if this situation occurs.
   *
   * \param influecervec Vector with the signature of a char[number_of_blocks]. Must be initialized with zeros, and output variables must be marked by setting the entry to 1.
   * \param callback The function callback(index) is called for every such variable.
   */
  template<typename influencervec_t, typename callback_t>
  void evaluate_for_typegrind(influencervec_t& influencervec, callback_t callback){
    iterate(number_of_blocks-1, 0, [&influencervec,&callback](ull index, ull index1, ull index2, double diff1, double diff2){
      if(influencervec[index]==1){
        if(index1 >= 0x8000000000000000) influencervec[index1] = 1;
        if(index2 >= 0x8000000000000000) influencervec[index2] = 1;
        if(index1>=0x8000000000000000 || index2>=0x8000000000000000){
          callback(index);
          return;
        }
      }
    });
  }
};

