package net.ufoai.sf.maputils;

/**
 * SurfaceFlags.java
 * Created on 19 April 2008, 12:15
 * @author andy buckle
 */
public class SurfaceFlags {
    
    public static final int NODRAW=128, TRANS33=16, TRANS66=32, FLOW=64;
    public static final int TRANS_ANY = TRANS33 | TRANS66;
    int flags;
    /**  */
    public SurfaceFlags(int flagsPacked) {
        flags=flagsPacked;
    }
    
    public boolean isTransparent(){
        return (flags & TRANS_ANY)!=0;
    }
    
    public boolean isNodraw(){
        return (flags & NODRAW)!=0;
    }
    
    public void setNodraw(){
        flags= flags | NODRAW;
    }
    
    public void unsetNodraw(){
        flags =flags & (~NODRAW);
    }
    
    public String toString(){
        return ""+flags;
    }
    
    public static void main(String[] args) {
        SurfaceFlags sf=new SurfaceFlags(1);
        System.out.println(""+sf);
        sf.setNodraw();
        System.out.println(""+sf);
        sf.unsetNodraw();
        System.out.println(""+sf);
    }
    
}
