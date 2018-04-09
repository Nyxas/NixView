#include "loadthread.h"
#include <QVector>
#include <chrono>
#include <thread>


LoadThread::LoadThread(QObject *parent, unsigned int chunksize):
    QThread(parent) {
    abort = false;
    restart = false;
    this->chunksize = chunksize;
}

LoadThread::~LoadThread() {
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();
}


void LoadThread::run() {
    while(! abort) {

        mutex.lock();
        if(restart) {
            mutex.unlock();
            restart=false;
            continue;
        }

        nix::DataArray array = this->array;
        nix::NDSize start = this->start;
        nix::NDSize extent = this->extent;
        unsigned int chunksize = this->chunksize;
        int graphIndex = this->graphIndex;
        nix::Dimension dim = this->dim;
        mutex.unlock();

        int dimCount = array.dataExtent().size();;

        if(dimCount == 1) {
            try{
            load1D(array, start, extent, dim, chunksize, graphIndex);
            } catch (...) {
                std::cerr << "probably obvious" << std::endl;
            }
        } else if(dimCount == 2) {
            mutex.lock();
            unsigned int dimNumber = this->dimNumber;
            std::vector<int> index2D = this->index2D;
            mutex.unlock();

            load2D(array, start, extent, dim, dimNumber, index2D, chunksize, graphIndex);
        }

        mutex.lock();
        if(! restart) {
            condition.wait(&mutex);
        } else {
            restart = false;
        }
        mutex.unlock();
    }
}


void LoadThread::load1D(nix::DataArray array, nix::NDSize start, nix::NDSize extent, nix::Dimension dim, unsigned int chunksize, int graphIndex) {

    int dataLength = extent[0];
    unsigned int offset = start[0];
    int totalChunks = (dataLength / chunksize) + 1;

    if( dataLength / chunksize == static_cast<double>(dataLength) / chunksize) {
        totalChunks = dataLength / chunksize;
    }

    extent[0] = chunksize;
    QVector<double> loadedData;
    std::vector<double> chunkdata(chunksize);

    bool brokenData = false;
    for (int i=0; i<totalChunks; i++) {
        mutex.lock();
        if(restart) {
            brokenData = true;
            mutex.unlock();
            break;
        }
        mutex.unlock();

        emit(progress(static_cast<double>(i)/totalChunks, graphIndex)); //starts with 0 ends with one step below 1

        if(i == totalChunks-1) {
            extent[0] = (dataLength - (totalChunks-1) * chunksize);
            chunkdata.resize((dataLength - (totalChunks-1) * chunksize));
        }
        start[0] = offset + i * chunksize;
        array.getData(chunkdata,extent, start);

        loadedData.append(QVector<double>::fromStdVector(chunkdata));
    }

    if(! brokenData) {
        QVector<double> axis(0);
        getAxis(dim, axis, dataLength, offset);

        emit dataReady(loadedData, axis, graphIndex);
    }
}


void LoadThread::load2D(nix::DataArray array, nix::NDSize start, nix::NDSize extent, nix::Dimension dim, unsigned int xDim, std::vector<int> index2D, unsigned int chunksize, int graphIndex) {

    unsigned int xDimIndex = xDim-1;
    unsigned int dataLength = extent[xDimIndex];
    unsigned int offset = start[xDimIndex];

    int totalChunks;
    if( dataLength / chunksize == static_cast<double>(dataLength) / chunksize) {
        totalChunks = dataLength / chunksize;
    } else {
        totalChunks = dataLength / chunksize + 1;
    }

    extent[xDimIndex] = chunksize;
    QVector<double> chunkdata(chunksize);

    //if index2D is empty do all.
    if(index2D.size() == 0) {
        index2D.resize(array.dataExtent()[1-xDimIndex]);
        for(unsigned int i=0; i< array.dataExtent()[1-xDimIndex]; i++) {
            index2D[i] = static_cast<int>(i);
        }
    }

    //iterate over given indices
    for(unsigned int j=0; j<index2D.size(); j++) {
        bool brokenData = false;
        QVector<double> loadedData;

        start[1-xDimIndex] = index2D[j];

        mutex.lock();
        if(restart) {
            brokenData = true;
            mutex.unlock();
            break;
        }
        mutex.unlock();

        for (int i=0; i<totalChunks; i++) {
            mutex.lock();
            if(restart) {
                brokenData = true;
                mutex.unlock();
                break;
            }
            mutex.unlock();

            emit(progress(static_cast<double>(i)*j / (totalChunks*index2D.size()), graphIndex)); //starts with 0 ends with one step below 1

            if(i == totalChunks-1) {
                extent[xDimIndex] = (dataLength - (totalChunks-1) * chunksize);
                chunkdata.resize((dataLength - (totalChunks-1) * chunksize));
            }
            start[xDimIndex] = offset + i * chunksize;

            array.getData(array.dataType(),chunkdata.data(),extent, start);

            loadedData.append(chunkdata);
            }

        if(! brokenData) {
            QVector<double> axis(0);
            getAxis(dim, axis, dataLength, offset);

            emit dataReady(loadedData, axis, graphIndex + index2D[j]);
        }
    }
}



void LoadThread::getAxis(nix::Dimension dim, QVector<double> &axis, unsigned int count, unsigned int offset) {

    if(dim.dimensionType() == nix::DimensionType::Sample) {
        axis = QVector<double>::fromStdVector(dim.asSampledDimension().axis(count, offset));
    } else if(dim.dimensionType() == nix::DimensionType::Range) {
        axis = QVector<double>::fromStdVector(dim.asRangeDimension().axis(count, offset));
    } else {
        axis.resize(count);
        for (unsigned int i=0; i<count; i++) {
            axis[0] = i+offset;
        }
    }
}


void LoadThread::setVariables(const nix::DataArray &array, nix::NDSize start, nix::NDSize extent, nix::Dimension dim, std::vector<int> index2D, unsigned int dimNumber, int graphIndex) {

    if(! testInput(array, start, extent)) {
        std::cerr << "LoadThread::setVariables(): Input not correct." << std::endl;
        return;
    }

    QMutexLocker locker(&mutex); // locks the members and unlocks them when it goes out of scope.

    this->array = nix::DataArray(array);
    this->start = start;
    this->extent = extent;
    this->graphIndex = graphIndex;
    this->index2D = index2D;
    this->dimNumber = dimNumber;
    this->dim = dim;

    if(! isRunning()) {
        QThread::start(LowPriority);
    } else {
        this->restart = true;
        condition.wakeOne();
    }
}

void LoadThread::setVariables1D(const nix::DataArray &array, nix::NDSize start, nix::NDSize extent, nix::Dimension dim, int graphIndex) {

    if(array.dataExtent().size() != 1) {
        std::cerr << "LoadThread::setVariables1D() given array has more than 1 dimension." << std::endl;
    }

    QMutexLocker locker(&mutex);

    this->array = nix::DataArray(array);
    this->start = start;
    this->extent = extent;
    this->dim = dim;
    this->graphIndex = graphIndex;

    if(! isRunning()) {
        QThread::start(LowPriority);
    } else {
        this->restart = true;
        condition.wakeOne();
    }
}

void LoadThread::setChuncksize(unsigned int size) {
    if(size == 0) {
        std::cerr << "LoadThread::setChunksize(): Size can't be zero." << std::endl;
        return;
    }

    mutex.lock();
    chunksize = size;
    mutex.unlock();
}


bool LoadThread::testInput(const nix::DataArray &array, nix::NDSize start, nix::NDSize extent) {
    nix::NDSize size = array.dataExtent();
    if( ! (size.size() == start.size() && size.size() == extent.size())) {
        std::cerr << "DataThread::testInput(): start and/or extent don't have the same dimensionality as the array." << std::endl;
        return false;
    }

    bool Dataload1d = false;
    for(uint i=0; i<size.size(); i++) {
        if(size[i] < start[i]+extent[i]) {
            std::cerr << "DataThread::testInput(): start + extent bigger than length of array in dimension " << i << std::endl;
            return false;
        }

        if(extent[i] > 1) { // extent has to define 1d data (all 1 exept for one entry)
            if (Dataload1d) {
                std::cerr << "DataThread::testInput(): extent defines data that is more than one dimensional." << std::endl;
                return false;
            } else {
                Dataload1d = true;
            }
        }
    }
    if(! Dataload1d) {
        std::cerr << "DataThread::testInput(): using DataThread to load a single datum." << std::endl;
    }

    return true;
}