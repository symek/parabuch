#ifndef __VEX_Pointgrid_h__
#define __VEX_Pointgrid_h__

// Thread safty:
class Locker: public UT_Lock
{
public: 
    Locker(){};
    ~Locker() {};
    void advance() {counter++;};
    void rewind()  {counter--;};
    int  get()     {return counter;};
    std::map<int,IMG_DeepPixelReader*> pixels;    
private:
    int counter;    
};

#endif
