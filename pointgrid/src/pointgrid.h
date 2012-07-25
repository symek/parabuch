#ifndef __pointgrid_h__
#define __pointgrid_h__

// Thread safty:
class Locker: public UT_Lock
{
public: 
    Locker(){};
    ~Locker() {};
    void advance() {counter++;};
    void rewind()  {counter--;};
    int  get()     {return counter;};
    //std::map<int,IMG_DeepPixelReader*> pixels;    
private:
    int counter;    
};

// Simple timer class:
class Timer 
{
private:
    double start_time;
public:
    void start()     { start_time = clock(); }
    double current() { return (difftime(clock(), start_time) / CLOCKS_PER_SEC);}
    bool is_timeout(double seconds) { return seconds >= current();}
};


typedef  UT_PointGridVector3ArrayAccessor<int, int> UT_Vector3Point;
typedef  UT_PointGrid<UT_Vector3Point>::queuetype UT_Vector3PointQueue;

// Worker class for multithreading:
class op_FillDCM
{
public:
    op_FillDCM(int res, IMG_DeepShadow *dcm,  
               UT_PointGrid<UT_Vector3Point> *pointgrid,  
               UT_Vector3Array *positions, Locker *locker): 
               myres(res), mydcm(dcm), mypointgrid(pointgrid), 
               mypositions(positions), mylocker(locker) {};

    void operator()(const UT_BlockedRange<int> &range) const
    {
        UT_Vector3PointQueue *queue;
        queue = mypointgrid->createQueue();
        UT_PointGridIterator<UT_Vector3Point> iter;
        //cout << "Range: " << range.begin() << ", ";
        for (int z = range.begin(); z != range.end(); ++z)
        {
            for (int y = 0; y < myres; y++)
            {
                for (int x = 0; x < myres; x++)
                {
                    iter = mypointgrid->getKeysAt(x, y, z, *queue);
                    if (iter.entries() != 0)
                    {
                        int _z = z*myres;
                        //mylocker->lock();
                        mydcm->pixelStart(_z + x, y);
                        float zdepth = 0;
                        for (;!iter.atEnd(); iter.advance())
                        {
                            int idx = iter.getValue();                        
                            UT_Vector3 pos = (*mypositions)(idx);
                            float posf[3]; posf[0] = pos.x(); 
                            posf[1] = pos.y(); posf[2] = pos.z();
                            mydcm->pixelWriteOrdered(0.1*zdepth, posf, 3);
                            zdepth++;
                        }
                        mydcm->pixelClose();
                        //mylocker->unlock();
                    }             
                }        
            }
        }
    }
private:
    int     myres;
    Locker *mylocker;
    IMG_DeepShadow                *mydcm;
    UT_PointGrid<UT_Vector3Point> *mypointgrid;
    UT_Vector3Array               *mypositions;

};

void
parallel_fillDCM(int res, IMG_DeepShadow *dcm,  UT_PointGrid<UT_Vector3Point> *pointgrid,  
                 UT_Vector3Array *positions, Locker *locker)
{
 UTparallelFor(UT_BlockedRange<int>(0, res), op_FillDCM(res, dcm, pointgrid, positions, locker), 0, res); // 'res' basically means no threading...
}


#endif
