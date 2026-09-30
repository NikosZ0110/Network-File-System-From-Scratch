package client;

import java.util.ArrayList;
import java.util.List;

public interface ClientConstants {
    long BLOCK_SIZE = 512;

    class MetadataNode {
        int virtualFd;
        int fd;
        String filename;
        long size;
        long cursorPos;
        int tmod;
        int version;
        static int virtualFdCnt = 0;

        public MetadataNode(int fd, String filename, long size, long cursorPos, int tmod, int version) {
            this.virtualFd = virtualFdCnt++;
            this.fd = fd;
            this.filename = filename;
            this.size = size;
            this.cursorPos = cursorPos;
            this.tmod = tmod;
            this.version = version;
        }
    }

    class CacheBlock {
        String filename;
        long start; // from which byte of the file does this block start storing data
        long end; // until which byte of the file does this block end storing data
        byte[] data; // data stored in a cache block
        long freshStamp;
        CacheBlock next;

        public CacheBlock(String filename, long start, long end, byte[] data) {
            this.filename = filename;
            this.start = start;
            this.end = end;
            this.data = data;
            this.freshStamp = System.currentTimeMillis();
            this.next = null;
        }
    }

    class Cache {
        private CacheBlock head;
        private int length;
        private final int cacheBlocks;

        public Cache(int cacheBlocks) {
            head = null;
            length = 0;
            this.cacheBlocks = cacheBlocks;
        }

        public CacheBlock getHead() {
            return head;
        }

        public void addToCache(String filename, int start, int endByte, byte[] data) {

            CacheBlock newBlock = new CacheBlock(filename, start, endByte, data);
            CacheBlock current = head;
            CacheBlock blockWithMinTimeStamp = head;

            if (length >= cacheBlocks && head != null) {
                while (current.next != null) {
                    if (current.freshStamp < blockWithMinTimeStamp.freshStamp) {
                        blockWithMinTimeStamp = current;
                    }
                    current = current.next;
                }
                // found out which node to remove since cache is full
                int index = findIndex(getHead(), current);
                removeNodeByIndex(head, index);
                length--;
            }

            if (head == null || compare(filename, start, head.filename, head.start) < 0) {
                // If the list is empty or the new node should be inserted before the head
                newBlock.next = head;
                head = newBlock;
                return;
            }

            current = head;
            while (current.next != null &&
                    (compare(filename, start, current.next.filename, current.next.start) > 0)) {
                // Traverse the list until reaching a node where the new node should be inserted
                // after
                current = current.next;
            }

            if (current.next == null) {
                int compareRes = compare(filename, start, current.filename, current.start);
                if (compareRes == 0) {
                    newBlock.next = null;
                    current = newBlock;
                } else if (compareRes > 0) {
                    newBlock.next = null;
                    current.next = newBlock;
                } else {
                    newBlock.next = current;
                    current = newBlock;
                }
            } else if (compare(filename, start, current.next.filename, current.next.start) == 0) {
                newBlock.next = current.next.next;
                current.next = newBlock;
                length--;
            } else {
                // Insert the new node after the current node
                newBlock.next = current.next;
                current.next = newBlock;
            }
            length++;
        }

        // Helper method to compare two cache block positions by filename and start byte
        private int compare(String filename1, long start1, String filename2, long start2) {
            int filenameCompare = filename1.compareTo(filename2);
            if (filenameCompare != 0) {
                return filenameCompare;
            }
            // If filenames are the same, compare by start byte
            return Long.compare(start1, start2);
        }

        public byte[] readFromCache(String filename, long start, long endByte) {

            CacheBlock current = head;

            while (current != null && current.start <= endByte && current.filename.equals(filename)) {
                if (current.start <= start && current.end >= endByte) {
                    if ((System.currentTimeMillis() - current.freshStamp) > (Client.freshT * 1000)) {
                        int index = findIndex(getHead(), current);
                        removeNodeByIndex(head, index);
                        return null;
                    }
                    // Found the range in the cache
                    long offset = start - current.start;
                    long length = endByte - start + 1;
                    byte[] result = new byte[(int) length];
                    System.arraycopy(current.data, Math.toIntExact(offset), result, 0, (int) length);
                    return result;
                }
                current = current.next;
            }

            // Range not found in the cache
            System.out.println("Requested bytes not found in the cache: " + start + " to " + endByte);
            return null;
        }

        public void removeBlocks(String filename) {

            int blocksRemoved = 0;

            if (head == null) {
                return; // If the list is empty, do nothing
            }
            while (head != null && head.filename.equals(filename)) {
                head = head.next; // Remove head nodes with matching filename
                blocksRemoved++;
            }
            CacheBlock current = head;
            while (current != null && current.next != null) {
                if (current.next.filename.equals(filename)) {
                    current.next = current.next.next; // Skip nodes with matching filename
                    blocksRemoved++;
                } else {
                    current = current.next;
                }
            }

            length -= blocksRemoved;
        }

        public int findIndex(CacheBlock head, CacheBlock node) {
            if (head == null || node == null)
                return -1; // Invalid input

            int index = 0;
            CacheBlock current = head;
            while (current != null) {
                if (current == node) {
                    return index;
                }
                current = current.next;
                index++;
            }
            return -1; // Node not found
        }

        // Method to remove a node by its index
        public void removeNodeByIndex(CacheBlock head, int index) {
            if (head == null || index < 0)
                return;

            if (index == 0) {
                this.head = head.next;
                return;
            }

            CacheBlock prev = null;
            CacheBlock current = head;
            int currentIndex = 0;
            while (current != null && currentIndex < index) {
                prev = current;
                current = current.next;
                currentIndex++;
            }

            if (current != null) {
                prev.next = current.next;
            } else {
                prev.next = null;
            }
        }

        public List<long[]> breakRangeIntoChunks(long start, long end, long base) {

            List<long[]> chunks = new ArrayList<>();
            long lastCurrStart = 0;
            long currentStart = (start / base) * base;

            while (currentStart <= end) {
                long currentEnd = Math.min(currentStart + base - 1, end);
                chunks.add(new long[] { currentStart, currentEnd });
                lastCurrStart = currentStart;
                currentStart = currentEnd + 1;
            }

            chunks.getLast()[1] = lastCurrStart + base - 1;

            return chunks;
        }
    }

    class Monitor {
    }
}
