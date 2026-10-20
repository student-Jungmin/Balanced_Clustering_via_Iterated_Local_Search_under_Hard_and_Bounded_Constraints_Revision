package harness;

import model.Instance;
import model.Result;
import other.RandomManager;
import solver.Algorithm;
import solver.GenericAlgorithmMulti;
import solver.TimedAlgorithm;
import solver.create.GRASPConstructor;
import solver.improve.FirstImpLS;
import solver.improve.StrategicOscillation;

import java.io.FileWriter;
import java.io.PrintStream;
import java.io.OutputStream;
import java.util.Set;
import java.util.concurrent.TimeUnit;

public class Runner {

    public static void main(String[] args) throws Exception {
        if (args.length < 6) {
            System.err.println("usage: Runner <csv> <n> <d> <k> <seed> <labels-out> [--budget SECONDS]");
            System.exit(2);
        }
        final String csv = args[0];
        final int n = Integer.parseInt(args[1]);
        final int d = Integer.parseInt(args[2]);
        final int k = Integer.parseInt(args[3]);
        final long seed = Long.parseLong(args[4]);
        final String labelsOut = args[5];

        double budgetSeconds = -1;
        for (int i = 6; i < args.length - 1; i++) {
            if (args[i].equals("--budget")) {
                budgetSeconds = Double.parseDouble(args[i + 1]);
            }
        }

        final PrintStream realOut = System.out;
        System.setOut(new PrintStream(new OutputStream() {
            public void write(int b) { System.err.write(b); }
        }, true));

        RandomManager.setSeed(seed);

        Instance instance = new Instance(new String[]{csv, String.valueOf(n),
                                                      String.valueOf(d), String.valueOf(k)});

        Algorithm algorithm;
        if (budgetSeconds > 0) {
            long nanos = (long) (budgetSeconds * 1e9);
            algorithm = new TimedAlgorithm(nanos, TimeUnit.NANOSECONDS,
                    () -> new GRASPConstructor(0.75, 0.75f),
                    FirstImpLS::new, StrategicOscillation::new, FirstImpLS::new);
        } else {
            algorithm = new GenericAlgorithmMulti(100, new GRASPConstructor(0.75, 0.75f),
                    new FirstImpLS(), new StrategicOscillation(), new FirstImpLS());
        }

        Result result = algorithm.execute(instance);

        int[] labels = new int[n];
        java.util.Arrays.fill(labels, -1);
        Set<Integer>[] clusters = result.s.getSolution();
        for (int c = 0; c < clusters.length; c++) {
            for (int point : clusters[c]) {
                labels[point] = c;
            }
        }
        for (int i = 0; i < n; i++) {
            if (labels[i] < 0) {
                System.err.println("[ERROR] point " + i + " was left unassigned");
                System.exit(1);
            }
        }

        try (FileWriter w = new FileWriter(labelsOut)) {
            StringBuilder sb = new StringBuilder();
            for (int label : labels) {
                sb.append(label).append('\n');
            }
            w.write(sb.toString());
        }

        realOut.printf("RESULT %.17g %.9f%n",
                       result.s.getOptimalValue(), result.getExecTime() / 1e9);
    }
}
