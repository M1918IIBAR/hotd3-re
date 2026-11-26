// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ghidra Script: Dump all string cross-references to a CSV file.
//
// Usage:
//   Script Manager → Run → hotd3_dump_strings.java
//   Output: <Ghidra project dir>/hotd3_strings.csv
//
// This script iterates every defined string in the program, records:
//   address, length, value (truncated to 120 chars), xref addresses
//
// Use the CSV to discover unknown function names, file paths, and
// error/debug messages that anchor further RE work.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.symbol.Reference;

import java.io.FileWriter;
import java.io.PrintWriter;

public class hotd3_dump_strings extends GhidraScript {

    @Override
    public void run() throws Exception {
        String outputPath = System.getProperty("user.home") + "/hotd3_strings.csv";
        println("Writing strings to: " + outputPath);

        int count = 0;
        try (PrintWriter out = new PrintWriter(new FileWriter(outputPath))) {
            out.println("address,length,string,xrefs");

            DataIterator it = currentProgram.getListing().getDefinedData(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Data d = it.next();
                if (!(d.getDataType() instanceof StringDataType)) continue;

                String value = d.getDefaultValueRepresentation();
                // Strip outer quotes added by Ghidra's representation
                if (value.startsWith("\"") && value.endsWith("\"")) {
                    value = value.substring(1, value.length() - 1);
                }
                if (value.length() > 120) {
                    value = value.substring(0, 120) + "...";
                }

                // Escape CSV
                value = "\"" + value.replace("\"", "\"\"") + "\"";

                StringBuilder xrefs = new StringBuilder();
                for (Reference ref : getReferencesTo(d.getAddress())) {
                    if (xrefs.length() > 0) xrefs.append("|");
                    xrefs.append("0x").append(ref.getFromAddress().toString());
                }

                out.printf("0x%s,%d,%s,%s%n",
                    d.getAddress().toString(),
                    d.getLength(),
                    value,
                    xrefs.length() > 0 ? xrefs.toString() : "(none)");

                count++;
            }
        }

        println(String.format("Exported %d strings to %s", count, outputPath));
    }
}
