import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class ExportMpSyncDecomp extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String outDir = args[0];
        new File(outDir).mkdirs();
        println("PROGRAM name=" + currentProgram.getName()
            + " path=" + currentProgram.getExecutablePath()
            + " imageBase=" + currentProgram.getImageBase());

        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);

        for (int i = 1; i < args.length; i++) {
            String arg = args[i];
            try {
                if (arg.equals("str") && i + 1 < args.length) {
                    dumpStrings(outDir, args[++i]);
                    continue;
                }
                exportFunction(di, outDir, arg);
            } catch (Exception ex) {
                println("ERR " + arg + ": " + ex);
            }
        }
        di.dispose();
        println("EXPORT DONE -> " + outDir);
    }

    private void exportFunction(DecompInterface di, String outDir, String arg) throws Exception {
        Address a = currentProgram.getAddressFactory().getAddress(arg);
        Function f = getFunctionContaining(a);
        if (f == null) {
            println("NOFUNC at " + arg);
            return;
        }
        Address ep = f.getEntryPoint();
        byte[] head = getBytes(ep, 8);
        StringBuilder hb = new StringBuilder();
        for (byte b : head) {
            hb.append(String.format("%02X ", b));
        }
        println("FUNC " + f.getName() + " @ " + ep + " head " + hb.toString().trim()
            + " (requested " + arg + ")");

        DecompileResults res = di.decompileFunction(f, 240, monitor);
        String c = (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null)
            ? res.getDecompiledFunction().getC()
            : ("// DECOMPILE FAILED: " + (res == null ? "null" : res.getErrorMessage()));
        String fname = f.getName() + "_" + ep.toString().toLowerCase() + ".c";
        try (PrintWriter w = new PrintWriter(new File(outDir, fname), "UTF-8")) {
            w.println("// " + f.getName() + " @ " + ep + "  (requested " + arg
                + ", first bytes: " + hb.toString().trim() + ")");
            w.println();
            w.print(c);
        }

        StringBuilder cl = new StringBuilder();
        for (Reference ref : getReferencesTo(ep)) {
            Function cf = getFunctionContaining(ref.getFromAddress());
            cl.append("  ").append(ref.getFromAddress()).append(" ").append(ref.getReferenceType())
                .append(" in ").append(cf == null ? "?" : cf.getName() + " @ " + cf.getEntryPoint())
                .append("\n");
        }
        try (PrintWriter w = new PrintWriter(new File(outDir, fname + ".callers.txt"), "UTF-8")) {
            w.print(cl.toString());
        }
    }

    private void dumpStrings(String outDir, String needle) throws Exception {
        String lower = needle.toLowerCase();
        List<String> lines = new ArrayList<>();
        DataIterator it = currentProgram.getListing().getDefinedData(true);
        while (it.hasNext()) {
            Data d = it.next();
            if (!d.hasStringValue()) {
                continue;
            }
            Object raw = d.getValue();
            if (!(raw instanceof String)) {
                continue;
            }
            String v = (String) raw;
            if (v == null || !v.toLowerCase().contains(lower)) {
                continue;
            }
            StringBuilder line = new StringBuilder(d.getAddress().toString());
            String shown = v.length() > 160 ? v.substring(0, 160) + "..." : v;
            line.append("  [").append(shown).append("]");
            boolean any = false;
            for (Reference ref : getReferencesTo(d.getAddress())) {
                Function cf = getFunctionContaining(ref.getFromAddress());
                line.append("\n      ref ").append(ref.getFromAddress()).append(" ")
                    .append(ref.getReferenceType()).append(" in ")
                    .append(cf == null ? "?" : cf.getName() + " @ " + cf.getEntryPoint());
                any = true;
            }
            if (!any) {
                line.append("\n      (no refs)");
            }
            lines.add(line.toString());
            if (lines.size() >= 250) {
                lines.add("... (capped at 250)");
                break;
            }
        }
        String safe = needle.replaceAll("[^A-Za-z0-9_-]", "_");
        try (PrintWriter w = new PrintWriter(new File(outDir, "strings_" + safe + ".txt"), "UTF-8")) {
            for (String l : lines) {
                w.println(l);
            }
        }
        println("STRINGS '" + needle + "': " + lines.size() + " hits -> strings_" + safe + ".txt");
    }
}
