#pragma once

#include <math.h>
#include <string>
#include <sys/stat.h>
#include <sstream>

/**
 * Conversion function which converts the string into the given type.
 *
 * @param string        The string representation
 *
 * @return  The value from the string representation
 */
template<typename TYPE>
TYPE parseType(const std::string& string) {
    TYPE value;

    std::stringstream ss(string);
    ss >> value;

    return value;
}

template<typename Number, typename NumberOff>
struct Problem {
  struct Settings {
    // values set by user
    size_t gridSize;
    size_t timeSteps;
    NumberOff R;
    NumberOff a;
    NumberOff b;
    NumberOff dT;

    // values which are computed from the above values
    size_t totalSize;
    size_t innerStart;
    size_t innerEnd;
    NumberOff oneOverR;
    NumberOff dX;
    NumberOff dTbyDX;
    NumberOff dTbyDX2;


    void updateDependentValues() {
      totalSize = gridSize * gridSize;
      innerStart = 1;
      if (0 != gridSize) {
        innerEnd = gridSize - 1;
      } else {
        innerEnd = 0;
      }

      oneOverR = 1.0 / R;

      NumberOff length = b - a;
      if (0 != gridSize) {
        dX = length / (NumberOff) (gridSize - 1);
      } else {
        dX = length;
      }
      dTbyDX = dT / dX;
      dTbyDX2 = dT / (dX * dX);
    }
  };

  Number *uStart;
  Number *vStart;
  Number *u1;
  Number *u2;
  Number *v1;
  Number *v2;

  long x;
  long t;

  int runs;
  std::string outDir;
  std::string prefix;


  inline NumberOff evalFuncU(const size_t xPos, const size_t yPos, const NumberOff t, const Settings& props) {
    NumberOff x = xPos * props.dX;
    NumberOff y = yPos * props.dX;

    return (x + y - 2.0 * x * t) / (1.0 - 2.0 * t * t);
  }

  inline NumberOff evalFuncV(const size_t xPos, const size_t yPos, const NumberOff t, const Settings& props) {
    NumberOff x = xPos * props.dX;
    NumberOff y = yPos * props.dX;

    return (x - y - 2.0 * y * t) / (1.0 - 2.0 * t * t);
  }

  inline void setBoundaryConditions(Number *u, Number *v, const NumberOff time, const Settings& props) {
    for (size_t gridPos = 0; gridPos < props.gridSize; ++gridPos) {
      size_t bx0 = gridPos;
      size_t bx1 = gridPos + props.innerEnd * props.gridSize;
      size_t b0y = gridPos * props.gridSize;
      size_t b1y = gridPos * props.gridSize + props.innerEnd;

      u[bx0] = evalFuncU(gridPos, 0, time, props);
      u[bx1] = evalFuncU(gridPos, props.innerEnd, time, props);
      u[b0y] = evalFuncU(0, gridPos, time, props);
      u[b1y] = evalFuncU(props.innerEnd, gridPos, time, props);

      v[bx0] = evalFuncV(gridPos, 0, time, props);
      v[bx1] = evalFuncV(gridPos, props.innerEnd, time, props);
      v[b0y] = evalFuncV(0, gridPos, time, props);
      v[b1y] = evalFuncV(props.innerEnd, gridPos, time, props);
    }
  }

  inline void setInitialConditions(Number *u, Number *v, const Settings& props) {
    for (size_t j = 0; j < props.gridSize; ++j) {
      for (size_t i = 0; i < props.gridSize; ++i) {
        size_t index = i + j * props.gridSize;

        u[index] = evalFuncU(i, j, 0.0, props);
        v[index] = evalFuncV(i, j, 0.0, props);
      }
    }
  }

  inline void updateField(Number *w_tp, const Number *w_t, const Number *u, const Number *v, const Settings& props) {
    // w_t + u*w_x + v*w_y = 1/R(w_xx + w_yy);
    Number velX;
    Number velY;
    Number vis;
    for (size_t j = props.innerStart; j < props.innerEnd; ++j) {
      for (size_t i = props.innerStart; i < props.innerEnd; ++i) {
        size_t index = i + j * props.gridSize;
        size_t index_xp = index + 1;
        size_t index_xm = index - 1;
        size_t index_yp = index + props.gridSize;
        size_t index_ym = index - props.gridSize;

        if (u[index] >= 0.0) {
          velX = u[index] * (w_t[index] - w_t[index_xm]);
        } else {
          velX = u[index] * ( w_t[index_xp] - w_t[index]);
        }
        if (v[index] >= 0.0) {
          velY = v[index] * (w_t[index] - w_t[index_ym]);
        } else {
          velY = v[index] * (w_t[index_yp] - w_t[index]);
        }

        vis = w_t[index_xp] + w_t[index_xm] + w_t[index_yp] + w_t[index_ym] - 4.0 * w_t[index];
        w_tp[index] = w_t[index] - props.dTbyDX * (velX + velY) + props.oneOverR * props.dTbyDX2 * vis;
      }
    }
  }

  inline void doStep(Number *u_cur, Number *u_next, Number *v_cur, Number *v_next, NumberOff& t,
                     const Settings& props) {
    updateField(u_next, u_cur, u_cur, v_cur, props); // update for u
    updateField(v_next, v_cur, u_cur, v_cur, props); // update for v
    t += props.dT;
    setBoundaryConditions(u_next, v_next, t, props);
  }

  void mainLoop(Number *u1, Number *u2, Number *v1, Number *v2, const Settings& props) {
    size_t timeEnd = props.timeSteps / 2; // we do two steps in each iteration
    NumberOff t = 0.0;
    for (size_t time = 0; time < timeEnd; ++time) {
      // first step
      doStep(u1, u2, v1, v2, t, props);

      // second step
      doStep(u2, u1, v2, v1, t, props);
    }
  }

  inline void computeL2Norm(const Number *u, const Number *v, const Settings& props, Number& rValue) {
    Number normU = 0.0;
    Number normV = 0.0;
    for (size_t j = props.innerStart; j < props.innerEnd; ++j) {
      for (size_t i = props.innerStart; i < props.innerEnd; ++i) {
        size_t index = i + j * props.gridSize;
        normU += u[index] * u[index];
        normV += v[index] * v[index];
      }
    }

    Number totalNorm = sqrt(normU) + sqrt(normV);
    rValue = totalNorm / props.totalSize;
  }

  void optimizedProblemSize(long& stackSize, long& varSize) {
    stackSize = 37 + -32 * x + 10 * x * x + t * (152 + -152 * x + 38 * x * x);
    varSize = 10 + -8 * x + 4 * x * x + t * (32 + -32 * x + 8 * x * x);
  }

  void problemSize(long& stackSize, long& varSize) {
    stackSize = 52 + -32 * x + 32 * x * x + t *(179 + -88 * x + 44 * x * x);
    varSize = 15 + -8 * x + 10 * x * x + t *(33 + -8 * x + 8 * x * x);
  }

  bool mkpath( std::string path ) {
    bool bSuccess = false;
    int nRC = ::mkdir( path.c_str(), 0775 );
    if( nRC == -1 ) {
      switch( errno ) {
        case ENOENT:
          //parent didn't exist, try to create it
          if( mkpath( path.substr(0, path.find_last_of('/')) ) ) {
            //Now, try to create again.
            bSuccess = 0 == ::mkdir( path.c_str(), 0775 );
          } else {
            bSuccess = false;
          }
          break;
        case EEXIST:
          //Done!
          bSuccess = true;
          break;
        default:
          bSuccess = false;
          break;
      }
    } else {
      bSuccess = true;
    }
    return bSuccess;
  }

  Settings setup(int nArgs, char** args) {
    if (nArgs != 4) {
      std::cerr << "Need 2 arguments: outputfile grid_size time_steps" << std::endl;
      exit(0);
    }

    x = parseType<long>(args[2]);
    t = parseType<long>(args[3]);

    Settings props;
    props.gridSize = x;
    props.timeSteps = t;
    props.R = 1.0;
    props.a = 0.0;
    props.b = 50.0;
    props.dT = 1e-4;
    props.updateDependentValues();

    uStart = new Number[props.totalSize];
    vStart = new Number[props.totalSize];
    u1 = new Number[props.totalSize];
    u2 = new Number[props.totalSize];
    v1 = new Number[props.totalSize];
    v2 = new Number[props.totalSize];

    setInitialConditions(uStart, vStart, props);

    return props;
  }

  void clear() {
    delete[] v2;
    delete[] v1;
    delete[] u2;
    delete[] u1;
    delete[] vStart;
    delete[] uStart;
  }
  /*
  void outputResults(Settings& props, int id, const double* times, const size_t timesSize, MemoryMeasurement& mem, const size_t* sizes, const size_t n) {
    std::string fileName = format("%s/%stimes_%02d.txt", outDir.c_str(), prefix.c_str(), id);
    FILE *file = fopen(fileName.c_str(), "a");
    fprintf(file, "%5lu %5lu", props.gridSize, props.timeSteps);
    for(size_t i = 0; i < n; ++i) {
      fprintf(file, " %5lu", sizes[i]);
    }
    for(size_t i = 0; i < timesSize; ++i) {
      fprintf(file, " %0.4e", times[i]);
    }
    fprintf(file, " %0.2f\n", (NumberOff) mem.physicalMemory / (1024.0 * 1024.0));
    fclose(file);
  }
  */
};
