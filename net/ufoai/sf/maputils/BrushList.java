package net.ufoai.sf.maputils;

import java.util.Vector;

/**
 * works out which brushes bounding boxes touch or intersect. Brushes may be in
 * worldspawn, func_group. brushes in func_breakable, rotating, ... entities are 
 * not included, as they do not lend themselves to optimisations.
 * transparent brushes are also not included.
 *
 * Also adds a list of brushes which might interact with a brush. this is stored
 * in the brush. the list is designed for optimisations. brushes with different levelflags will
 * not be included. 
 * (they can disappear, move, ...).
 * Created on 20 April 2008, 07:25
 * @author andy buckle
 */
public class BrushList {
    
    /** a list of immutable (not breakable or mobile) brushes from 
     *  various entities. */
    static Vector<Brush> brushes=new Vector<Brush>(100,100);
        
    /**  */
    public static void go(Map map) {
        if(brushes.size()!=0) return;//already been done
        //System.out.println("BrushList:go");
        //get a reference to each Brush in; worldspawn, func_group, ...
        TimeCounter.report("started making Brush potential interaction list.");
        Vector<Entity> entities=map.getEntities();
        for(Entity ent:entities){
            if(ent.hasImmutableBrushes()){
                Vector<Brush> entityBrushes=ent.getBrushes();
                for(Brush b:entityBrushes) {
                    if(!b.hasTransparentFaces()  && !b.isActorclipWeaponClipOrStepon() ) brushes.add(b);
                    //System.out.println("adding a brush");
                }
            }
        }
        //for each brush, make a list of brushes that might touch it, and store it in the brush
        for(int i=0;i<brushes.size();i++){
            for(int j=0;j<brushes.size();j++){//j needs a ref to i and i needs a ref to j
                Brush bi=brushes.get(i), bj=brushes.get(j);
                if(i!=j && 
                   bi.boundingBoxIntersects(bj) && 
                   bi.getLevelFlags()==bj.getLevelFlags()) {
                    bi.addToInteractionList(bj);
                }
            }
        }
        TimeCounter.report("finished making Brush potential interaction list.");
    }
    
    public static Vector<Brush> getImmutableOpaqueBrushes(){
        return brushes;
    }
    
}
