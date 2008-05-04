package net.ufoai.sf.maputils;

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.util.Vector;
import javax.swing.JFrame;
import javax.swing.JPanel;

/**
 * OrthographicProjectionFrame.java
 * Created on 23 April 2008, 08:30
 * @author andy buckle
 */
public class OrthographicProjectionFrame extends JFrame {
    
    float xmin, xmax, ymin, ymax, zmin, zmax, xrange, yrange, zrange, range;
    int pixMargin;
    
    /** pixels/input coord units*/
    float scale=1.0f;
    /** added around each edge. a fraction of the range. default 0.1 */
    float margin=0.2f;
    Vector<Point> points=new Vector<Point>(20,20);
    Vector<Line> lines=new Vector<Line>(20,20);
    OrthographicProjectionPanel pane;
    boolean noPointsYet=true;
    
    
    /**  */
    public OrthographicProjectionFrame() {
        this("OrthographicProjection");
    }
    
    public OrthographicProjectionFrame(String title) {
        super(title);
        pane=new OrthographicProjectionPanel();
        this.setContentPane(pane);
        this.addWindowListener(new WindowAdapter(){
            public void windowClosing(WindowEvent e){
                System.exit(0);
            }
        });
        pack();
    }
    
    public void addLine(Vector3D start, Vector3D end, Color color){
        lines.add(new Line(start, end, color));
        resetBoundingBox(start);
        resetBoundingBox(end);
        resetScale();
    }
    
    public void addPoint(Vector3D coordinates, Color color, int diameter){
        Point p=new Point(coordinates, color, diameter);
        points.add(p);
        resetBoundingBox(coordinates);
        resetScale();
    }
    
    public void resetBoundingBox(Vector3D coordinates){
        if(noPointsYet){
            xmax=coordinates.x;
            xmin=coordinates.x;
            ymax=coordinates.y;
            ymin=coordinates.y;
            zmax=coordinates.z;
            zmin=coordinates.z;
            range=1.0f;
            noPointsYet=false;
        } else {
            xmax=xmax>coordinates.x?xmax:coordinates.x;
            xmin=xmin<coordinates.x?xmin:coordinates.x;
            ymax=ymax>coordinates.y?ymax:coordinates.y;
            ymin=ymin<coordinates.y?ymin:coordinates.y;
            zmax=zmax>coordinates.z?zmax:coordinates.z;
            zmin=zmin<coordinates.z?zmin:coordinates.z;
            xrange=xmax-xmin;
            yrange=ymax-ymin;
            zrange=zmax-zmin;
            range=xrange > yrange ? xrange :yrange;
            range=range>zrange ?range:zrange ;
        }
    }
    
    public void resetScale(){
        float pixPerQuarter=pane.getProjectionQuarterSize();
        scale=pixPerQuarter/(range*(1.0f+2.0f*margin));
        pixMargin=(int)(margin*range*scale);
        //System.out.printf("pixPerQuarter:%5.1f range:%5.1f%n",pixPerQuarter,range);
        pane.repaint();
    } 
    
    class OrthographicProjectionPanel extends JPanel {
    
        Dimension size=new Dimension(400,400);
        
    
        public Dimension getMinimumSize(){
            return size;
        }
        
        public Dimension getPreferredSize(){
            return size;
        }
        
        public float getProjectionQuarterSize(){
            return ((float)(size.height > size.width ? size.height :size.width ))/2.0f;
        }
        
        public void paint(Graphics g){
            //super.paint(g);
            int offset=(int)getProjectionQuarterSize();
            g.drawString("x",offset+pixMargin*2,2*offset-pixMargin/2);
            g.drawString("y",offset+pixMargin/2,2*offset-pixMargin*2);
            g.drawString("x",offset+pixMargin*2,offset-pixMargin/2);
            g.drawString("z",offset+pixMargin/2,offset-pixMargin*2);
            g.drawString("y",pixMargin*2,offset-pixMargin/2);
            g.drawString("z",pixMargin/2,offset-pixMargin*2);
            for(Line line:lines){
                int x1=(int)((line.p1.x-xmin)*scale);
                int y1=(int)((line.p1.y-ymin)*scale);
                int z1=(int)((line.p1.z-zmin)*scale);
                int x2=(int)((line.p2.x-xmin)*scale);
                int y2=(int)((line.p2.y-ymin)*scale);
                int z2=(int)((line.p2.z-zmin)*scale);
                //xy
                g.drawLine(x1+offset+pixMargin,2*offset-y1-pixMargin,
                        x2+offset+pixMargin,2*offset-y2-pixMargin);
                //yz
                g.drawLine(y1-pixMargin,offset-z1-pixMargin,
                        y2-pixMargin,offset-z2-pixMargin);
                //xz
                g.drawLine(x1+offset+pixMargin,offset-z1-pixMargin,
                        x2+offset+pixMargin,offset-z2-pixMargin);
            }
            for(Point p:points){ 
                int x=(int)((p.p.x-xmin)*scale);
                int y=(int)((p.p.y-ymin)*scale);
                int z=(int)((p.p.z-zmin)*scale);
                //xy
                g.drawOval(x+offset-p.radius+pixMargin,2*offset-y-p.radius-pixMargin,
                        p.diameter,p.diameter);
                //yz
                g.drawOval(y-p.radius+pixMargin,offset-z-p.radius-pixMargin,
                        p.diameter,p.diameter);
                //xz
                g.drawOval(x+offset-p.radius+pixMargin,offset-z-p.radius-pixMargin,
                        p.diameter,p.diameter);
            }
        }
        
    }
    
}
    


class Point {
    
    Vector3D p;
    Color c;
    int radius, diameter;
    
    public Point(Vector3D coordinates, Color color, int radiusInPixels){
        p=coordinates;
        c=color;
        radius=radiusInPixels;
        diameter=radius*2;
    }
    
}

class Line {
    
    Vector3D p1, p2;
    Color c;
    
    public Line(Vector3D coordinates1, Vector3D coordinates2, Color color){
        p1=coordinates1;
        p2=coordinates2;
        c=color;
    }
    
}


