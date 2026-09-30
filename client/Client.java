package client;

import java.io.IOException;
import java.net.*;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

public class Client implements ClientConstants {

    static DatagramSocket socket;
    static InetAddress serverAddress;
    static int serverPort, cacheBlocks;
    static List<MetadataNode> fileList;
    static Cache cache;
    static int incNum;
    static int reqNum;
    static boolean received;
    static String receivedData;
    static long freshT; // given in seconds by user, in milliseconds in java. Make sure to do appropriate math
    Receiver receiver;
    Thread receiverThread;
    public static final Object lock = new Object();

    public Client(String serverAddress, int port, int cacheBlocks, int freshT) throws SocketException, UnknownHostException {
        Client.socket = new DatagramSocket(port);
        Client.serverAddress = InetAddress.getByName(serverAddress);
        Client.serverPort = 8080;
        Client.cacheBlocks = cacheBlocks;
        System.out.println(Client.serverAddress); // here, <InetAddress> serverAddress does have the '/'
        Client.freshT = freshT;
        receiver = new Receiver();
        receiverThread = new Thread(receiver);
        receiverThread.start();
        reqNum = 0;
        received = false;
    }

    public void mynfs_init() {
        fileList = new LinkedList<>();
        cache = new Cache(Client.cacheBlocks); // to add a new block in cache: cache.put(<new_fd>, new byte[BLOCK_SIZE]);
        incNum = -1;
    }

    public void displayFileDescriptors() {
        for (MetadataNode node: fileList) {
            System.out.println(node.virtualFd + ": " + node.filename);
        }
    }

    public int mynfs_open(String input) throws IOException, InterruptedException {

        String[] tokens = input.split("\\s+");
        String svcid = tokens[0];
        String filename = tokens[1];
        MetadataNode tmpNode = null;

        for (MetadataNode node: fileList) {
            if (node.filename.equals(filename) && node.fd != -1) { return -2; }
            else if (node.filename.equals(filename)) {
                tmpNode = node;
                break;
            }
        } // 0 -> file already open

        String toSend = incNum + " " + svcid + " " + reqNum + " " + filename;
      

        byte[] sendData = toSend.getBytes();
        DatagramPacket packet = new DatagramPacket(sendData, sendData.length, Client.serverAddress, Client.serverPort);

        while (!received) {
            socket.send(packet);
            block();
        }

        reqNum++;
        received = false;
        String[] responseTokens = receivedData.split("\\s+");

        if (responseTokens.length == 1) {
            incNum = Integer.parseInt(responseTokens[0]);
            for (MetadataNode node : fileList) {
                node.fd = -1;
            }
             // incarnation number has changed, show server error to application level
            return mynfs_open(input);
        }

        if (Integer.parseInt(responseTokens[2]) == -1) {
            return -1;
        }

        MetadataNode newNode;
        int tmpFd = Integer.parseInt(responseTokens[2]);
        if (tmpNode != null && tmpNode.filename.equals(filename)) {
            tmpNode.fd = Integer.parseInt(responseTokens[2]);
            tmpNode.version = Integer.parseInt(responseTokens[3]);
            for (MetadataNode node: fileList) {
                if (!node.filename.equals(filename) && node.fd == tmpFd) {
                    node.fd = -1;
                }
            }
            return tmpNode.virtualFd;
        } else {
            incNum = Integer.parseInt(responseTokens[0]);
            int fdVersion = Integer.parseInt(responseTokens[3]);
            // String filename -> in the beginning of this method
            long tmpSize = Long.parseLong(responseTokens[4]);
            newNode = new MetadataNode(tmpFd, filename, tmpSize, 0, 0, fdVersion);
            fileList.add(newNode);
        }

        for (MetadataNode node: fileList) {
            if (!node.filename.equals(filename) && node.fd == tmpFd) {
                node.fd = -1;
            }
        }

        return newNode.virtualFd;
    }

    public long mynfs_read(int virtualFd, StringBuilder toRead, long length) throws IOException, InterruptedException { // input = "r incarnation_num fd(int) bytesToRead";

        long start, end;
        long tmpStart;
        byte[] result;

        for (MetadataNode node: fileList) {
            if (node.virtualFd != virtualFd) { continue; }
            if (node.fd == -1) {
                if (mynfs_open("o " + node.filename) == -1) {
                    return(-1);
                }
            }
            List<long[]> chunksToRead = cache.breakRangeIntoChunks(node.cursorPos, node.cursorPos + length - 1, BLOCK_SIZE);
            start = node.cursorPos;
            end = start + length - 1;
            tmpStart = start;
            for (long[] chunk: chunksToRead) {
                if (end <= chunk[1]) {
                    result = cache.readFromCache(node.filename, tmpStart, end);
                } else {
                    result = cache.readFromCache(node.filename, tmpStart, chunk[1]);
                }

                int i = 0;
                for (CacheBlock current = cache.getHead(); current != null; current = current.next) {
                    System.out.println(i + ": " + new String(current.data));
                    i++;
                }
                
                if (result == null) {
                    String toSend = incNum + " r " + reqNum + " " + node.fd + " " + node.version + " " + chunk[0] + " " + BLOCK_SIZE;

                    byte[] sendData = toSend.getBytes();
                    DatagramPacket packet = new DatagramPacket(sendData, sendData.length, Client.serverAddress, Client.serverPort);

                    while (!received) {
                        socket.send(packet);
                        block();
                    }

                    reqNum++;
                    received = false;
                    String receivedBytesToString = receivedData.toString();
                    String[] responseTokens = receivedData.split("\\s+");

                    if (responseTokens.length == 1) {
                        incNum = Integer.parseInt(responseTokens[0]);
                        for (MetadataNode node2: fileList) {
                            node2.fd = -1;
                        }
                        return mynfs_read(virtualFd, toRead, length);
                    } else if (responseTokens.length == 3) {
                        node.size = Long.parseLong(responseTokens[2]);
                        return -1;
                    } else if (responseTokens.length == 4) {
                        node.fd = -1;
                        return mynfs_read(virtualFd, toRead, length);
                    } else {
                        int endend;
                        String tmp = String.join(" ", Arrays.copyOfRange(responseTokens, 5, responseTokens.length));
                        System.out.println(" TMP ++==++ " + tmp);
                        System.out.println(" TO TMP EXEI SIZE ====== " + tmp.length());
                        if (receivedBytesToString.charAt(receivedBytesToString.length() - 1) == ' ') {
                            tmp += " ";
                        }
                        if (receivedBytesToString.charAt(receivedBytesToString.length() - 1) == '\n') {
                            tmp += "\n";
                        }
                        if (node.tmod == Integer.parseInt(responseTokens[3])) {
                            node.size = Long.parseLong(responseTokens[2]);
                            if (!tmp.isEmpty()) {
                                cache.addToCache(node.filename, (int)chunk[0], (int)chunk[0] + tmp.length() - 1, tmp.getBytes());
                                if (length + start > node.size) {
                                    endend = (int) node.size;
                                } else {
                                    endend = (int) (length + start);
                                }

                                if (tmpStart == node.size) {
                                    return 0;
                                }

                                try {
                                    if (endend > chunk[1]) {
                                        toRead.append(tmp, (int) (tmpStart - chunk[0]), (int) (BLOCK_SIZE));
                                        node.cursorPos += (BLOCK_SIZE) - (tmpStart - chunk[0]);
                                    } else {
                                        toRead.append(tmp, (int) (tmpStart - chunk[0]), (endend % 512));
                                        node.cursorPos += (long) (endend % 512) - (tmpStart - chunk[0]);
                                    }
                                } catch (Exception e) {}
                            }
                        } else {
                            cache.removeBlocks(node.filename);
                            node.tmod = Integer.parseInt(responseTokens[3]);
                            node.size = Long.parseLong(responseTokens[2]);
                            if (!tmp.isEmpty()) {
                                cache.addToCache(node.filename, (int) chunk[0], (int) chunk[0] + tmp.length() - 1, tmp.getBytes());
                            }
                            return mynfs_read(virtualFd, toRead, length);
                        }
                    }
                }
                else {
                    String toAdd = new String(result);
                    toRead.append(toAdd);
                    node.cursorPos += toAdd.length();
                }
                tmpStart = chunk[1] + 1;
            }

            return toRead.length();
        }


        return -2; // -2 -> file not open
    }

    public long mynfs_write(int virtualFd, String toWrite, long length) throws IOException, InterruptedException {

        for (MetadataNode node: fileList) {
            if (node.virtualFd != virtualFd) {
                continue;
            }
            if (node.fd == -1) {
                if (mynfs_open("o " + node.filename) == -1) {
                    return(-1);
                }
            }

            if (toWrite == null || length < 0 || length > toWrite.length()) {
                return -1;
            }

            String toSend = incNum + " w " + reqNum + " " +  node.fd + " " + node.version + " " + node.cursorPos + " " + toWrite.substring(0, (int)length);

            byte[] sendData = toSend.getBytes();
            DatagramPacket packet = new DatagramPacket(sendData, sendData.length, Client.serverAddress, Client.serverPort);

            while (!received) {
                socket.send(packet);
                block();
            }

            reqNum++;
            received = false;
            String[] responseTokens = receivedData.split("\\s+");

            if (responseTokens.length == 1) {
                incNum = Integer.parseInt(responseTokens[0]);
                for (MetadataNode node2: fileList) {
                    node2.fd = -1;
                }
                return mynfs_write(virtualFd, toWrite, length);
            } else if (responseTokens.length == 3) {
                return -1;
            } else if (responseTokens.length == 4) {
                node.fd = -1;
                return mynfs_write(virtualFd, toWrite, length);
            } else {
                cache.removeBlocks(node.filename);
                node.tmod = Integer.parseInt(responseTokens[3]);
                node.size = Long.parseLong(responseTokens[2]);
                node.cursorPos += Long.parseLong(responseTokens[4]);
                return Long.parseLong(responseTokens[4]);
            }
        }

        return -2;
    }

    public long mynfs_lseek(int virtualFd, long bytes, int whence) {

        for (MetadataNode node: fileList) {
            if (node.virtualFd != virtualFd) { continue; }

            /* whence: 0 -> SEEK_SET
                1 -> SEEK_CUR
                2 -> SEEK_END */

            switch (whence) {
                case 0:

                    if (bytes > node.size) {
                        node.cursorPos = node.size;
                        break;
                    }
                    if (bytes < 0) {
                        node.cursorPos = 0;
                        break;
                    }
                    node.cursorPos = bytes;

                    break;
                case 1:
                    if ((node.cursorPos + bytes) >= node.size) {
                        node.cursorPos = node.size;
                        break;
                    }
                    if ((node.cursorPos + bytes) < 0) {
                        node.cursorPos = 0;
                        break;
                    }
                    node.cursorPos += bytes;
                    break;
                case 2:
                    if (((node.size) + bytes) >= node.size) {
                        node.cursorPos = node.size;
                        break;
                    }
                    if (((node.size) + bytes) < 0) {
                        node.cursorPos = 0;
                        break;
                    }
                    node.cursorPos = node.size + bytes;
                    break;
            }

            return node.cursorPos;
        }

        return -1;
    }

    public int mynfs_close(int virtualFd) {

        for (MetadataNode node: fileList) {
            if (node.virtualFd != virtualFd) { continue; }
            cache.removeBlocks(node.filename); // removes all blocks with that filename
            fileList.remove(node);
            return 0;
        }

        return -1;
    }

    public static void block() throws InterruptedException {
        synchronized (lock) {
            lock.wait(1000);
        }
    }

    public static void unblock() {
        synchronized (lock) {
            lock.notify();
        }
    }
}
