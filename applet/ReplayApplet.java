import java.applet.Applet;
import java.awt.*;
import java.awt.geom.*;
import java.awt.image.*;
import java.net.*;
import java.io.*;
import java.util.*;

public class ReplayApplet extends Applet
{
    public static final int FIELD_SIZE = 2000;

    private int game_id, num_players;
    private int data_rate, turn_rate, move_rate, warmup,
                hole_prob, hole_size_min, hole_size_max, hole_cooldown;

    private BufferedImage bi = new BufferedImage(FIELD_SIZE, FIELD_SIZE, BufferedImage.TYPE_INT_RGB);

    private boolean digit(char ch)
    {
        return ch >= '0' && ch <= '9';
    }

    private String readString(BufferedReader br)
    {
        try {
            return br.readLine();
        } catch (IOException ioe) {
            return null;
        }
    }

    private int[] readInts(BufferedReader br)
    {
        String line;
        try {
            line = br.readLine();
        } catch (IOException ioe) {
            return null;
        }
        if (line == null) return null;

        ArrayList<Integer> ints = new ArrayList<Integer>();
        boolean minus = false;
        for (int n = 0; n < line.length(); ++n)
        {
            if (digit(line.charAt(n)))
            {
                int m = n, i = 0;
                while (m < line.length() && digit(line.charAt(m)))
                {
                    i = 10*i + line.charAt(m) - '0';
                    m += 1;
                }
                n = m;
                ints.add(minus ? -i : i);
            }
            if (n < line.length()) minus = line.charAt(n) == '-';
        }
        int[] res = new int[ints.size()];
        for (int n = 0; n < res.length; ++n) res[n] = ints.get(n);
        return res;
    }

    private void print(int[] ints)
    {
        System.out.print("[ ");
        for (int i = 0; i < ints.length; ++i)
        {
            if (i > 0) System.out.print(", ");
            System.out.print(ints[i]);
        }
        System.out.println(" ]");
    }

    private void readReplay(URL url) throws IOException
    {
        BufferedReader br = new BufferedReader(new InputStreamReader(url.openStream()));
        int[] ints = readInts(br);
        game_id = ints[1];
        num_players = ints[2];

        ints = readInts(br);
        data_rate     = ints[0];
        turn_rate     = ints[1];
        move_rate     = ints[2];
        warmup        = ints[3];
        hole_prob     = ints[4];
        hole_size_min = ints[5];
        hole_size_max = ints[6];
        hole_cooldown = ints[7];

        String[] names = new String[num_players];
        for (int n = 0; n < num_players; ++n)
        {
            names[n] = readString(br);
        }

        double[] xs = new double[num_players],
                 ys = new double[num_players],
                 as = new double[num_players];
        for (int n = 0; n < num_players; ++n)
        {
            ints = readInts(br);
            xs[n] = ints[0]/65536.0;
            ys[n] = ints[1]/65536.0;
            as[n] = ints[2]/32768.0*Math.PI;
        }

        Graphics2D g = bi.createGraphics();
        AffineTransform xform = AffineTransform.getScaleInstance(1.0, -1.0);
        xform.translate(0.0, -1.0*FIELD_SIZE);
        g.transform(xform);
        while ((ints = readInts(br)) != null)
        {
            int i = ints[1];
            int a = ints[2];
            int v = ints[3];
            double na = as[i] + a*2*Math.PI/turn_rate;
            if (v > 0)
            {
                double nx = xs[i] + Math.cos(na)*1e-3*move_rate;
                double ny = ys[i] + Math.sin(na)*1e-3*move_rate;

                if (v == 1)
                {
                    double th = 7e-3;
                    int[] x = new int[4];
                    int[] y = new int[4];
                    x[0] = (int)(FIELD_SIZE*(xs[i] - 0.5*Math.sin(as[i])*th));
                    y[0] = (int)(FIELD_SIZE*(ys[i] + 0.5*Math.cos(as[i])*th));
                    x[1] = (int)(FIELD_SIZE*(nx - 0.5*Math.sin(na)*th));
                    y[1] = (int)(FIELD_SIZE*(ny + 0.5*Math.cos(na)*th));
                    x[2] = (int)(FIELD_SIZE*(nx + 0.5*Math.sin(na)*th));
                    y[2] = (int)(FIELD_SIZE*(ny - 0.5*Math.cos(na)*th));
                    x[3] = (int)(FIELD_SIZE*(xs[i] + 0.5*Math.sin(as[i])*th));
                    y[3] = (int)(FIELD_SIZE*(ys[i] - 0.5*Math.cos(as[i])*th));
                    g.setColor( i == 0 ? Color.YELLOW : Color.RED);
                    g.fillPolygon(x, y, 4);
                }
                xs[i] = nx;
                ys[i] = ny;
            }
            as[i] = na;
        }
    }

    public void init()
    {
        try {
            URL url = new URL(getParameter("file"));
            readReplay(url);
        } catch (MalformedURLException mue) {
            mue.printStackTrace();
        } catch (IOException ioe) {
            ioe.printStackTrace();
        }
    }

    public void paint(Graphics g)
    {
        g.drawImage(bi, 0, 0, getWidth(), getHeight(), 0, 0, FIELD_SIZE, FIELD_SIZE, Color.BLACK, null);
    }
}
