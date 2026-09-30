package client;

import java.io.IOException;
import java.util.Scanner;

public class App {
    public static void main(String[] Args) throws IOException, InterruptedException {

        Scanner scanner = new Scanner(System.in);
        String[] tokens;
        String input;
        String toWrite;
        Client client;
        char operation;
        long res;
        int whence;
        long start, end, diff;
        double totalTime;

        if (Args.length != 4) {
            System.out.println("Incorrect number of parameters...\nExiting..."); // Server address, port, cache blocks,
                                                                                 // freshT
            System.exit(69);
        }

        client = new Client(Args[0], Integer.parseInt(Args[1]), Integer.parseInt(Args[2]), Integer.parseInt(Args[3]));
        client.mynfs_init();
        /*
         * instructions for input:
         * "o <filename>" - open a file
         * "r <fd> <String> <int>" - read <int> bytes from <filename>
         * "w <fd>" - write <String> string in <filename> in position where cursor is
         * located
         * "l <fd> <long> <int>" - move cursor of <filename> <long> bytes at <String>
         * (String -> "start", "end", "here")
         * "c <fd>" - close <filename>
         */

        for (;;) {
            input = scanner.nextLine().trim();
            operation = input.charAt(0);

            switch (operation) {
                case 'o':
                    if (countWords(input) != 2) {
                        System.out.println("Wrong number of arguments.");
                        continue;
                    }
                    res = client.mynfs_open(input);

                    if (res == -2) {
                        System.out.println("File already open");
                    } else if (res == -1) {
                        System.out.println("There was an error in server. Consider reopening the file.");
                    } else {
                        System.out.println("File open successfully with file descriptor: " + res);
                    }
                    break;

                case 'r':
                    if (countWords(input) != 3) {
                        System.out.println("Wrong number of arguments.");
                        continue;
                    }
                    tokens = input.split("\\s+");
                    StringBuilder toPrint = new StringBuilder();
                    start = System.currentTimeMillis();
                    res = client.mynfs_read(Integer.parseInt(tokens[1]), toPrint, Long.parseLong(tokens[2]));
                    end = System.currentTimeMillis();
                    diff = end - start;
                    totalTime = (double) diff / 1000;

                    if (res == -1) {
                        System.out.println("There was an error in server. Consider resetting file cursor and rereading the file.");
                    } else if (res == -2) {
                        System.out.println("No such file open.");
                    } else {
                        System.out.println(toPrint + "\nRead " + res + " bytes.");
                    }
                    System.out.println("Read process took: " + totalTime + "secs");
                    break;

                case 'w':
                    if (countWords(input) != 2) {
                        System.out.println("Wrong number of arguments.");
                        continue;
                    }
                    System.out.println("Type the String you want to write to the file.");
                    tokens = input.split("\\s+");
                    toWrite = scanner.nextLine();
                    res = client.mynfs_write(Integer.parseInt(tokens[1]), toWrite, toWrite.length());

                    if (res == -1) {
                        System.out.println("There was an error in server. Consider rewriting to the file.");
                    } else if (res == -2) {
                        System.out.println("No such file open.");
                    } else {
                        System.out.println(res + " bytes written.");
                    }
                    break;

                case 'l':
                    if (countWords(input) != 4) {
                        System.out.println("Wrong number of arguments.");
                        continue;
                    }

                    tokens = input.split("\\s+");
                    if (tokens[3].equals("SEEK_SET")) {
                        whence = 0;
                    } else if (tokens[3].equals("SEEK_CUR")) {
                        whence = 1;
                    } else {
                        whence = 2;
                    }

                    res = client.mynfs_lseek(Integer.parseInt(tokens[1]), Integer.parseInt(tokens[2]), whence);

                    if (res == -1) {
                        System.out.println("No such file open.");
                    } else {
                        System.out.println("Cursor at byte " + res);
                    }

                    break;

                case 'c':
                    if (countWords(input) != 2) {
                        System.out.println("Wrong number of arguments.");
                        continue;
                    }
                    res = client.mynfs_close(Integer.parseInt(input.split("\\s+")[1]));
                    if (res == -1) {
                        System.out.println("No such file open.");
                    } else {
                        System.out.println("File closed.");
                    }
                    break;

                case 'd':
                    client.displayFileDescriptors();
                    break;

                case 'q':
                    System.exit(69);
                    scanner.close();
                    break;

                default:
                    System.out.println("Wrong operation argument.");
            }
        }
    }

    public static int countWords(String input) {
        if (input == null || input.isEmpty()) {
            return 0;
        }

        // Split the input string by spaces
        String[] words = input.split("\\s+");

        // Return the number of words
        return words.length;
    }
}
