package client;

import java.io.IOException;
import java.net.DatagramPacket;

public class Receiver implements Runnable {

    @Override
    public void run() {

        for (;;) {
            byte[] rcvData = new byte[1024];
            DatagramPacket rcvPacket = new DatagramPacket(rcvData, rcvData.length);

            try {
                Client.socket.receive(rcvPacket);
            } catch (IOException e) {
                e.printStackTrace();
            }

            String receivedData = new String(rcvPacket.getData(), 0, rcvPacket.getLength());
            String[] ReceivedDataTokens = receivedData.split(" ");

            if (ReceivedDataTokens.length == 1 || Client.reqNum == Integer.parseInt(ReceivedDataTokens[1])) {
                Client.receivedData = receivedData;
                Client.received = true;
                Client.unblock();
            }
        }
    }
}
