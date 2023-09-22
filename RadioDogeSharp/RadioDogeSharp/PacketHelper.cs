namespace RadioDoge
{
    internal static class PacketHelper
    {
        private const int MAX_PAYLOAD_BYTES = 192;

        internal static byte[] CreatePacket(NodeAddress destAddress, NodeAddress localAddress, byte[] payload)
        {
            List<byte> packetBytes = new List<byte>(payload.Length + 6);
            byte commandType = (byte)SerialCommandType.HostFormedPacket;
            byte messageSize = (byte)(payload.Length + 6);
            packetBytes.AddRange(CreateCommandHeader((byte)commandType, (byte)(messageSize + 2)));
            packetBytes.AddRange(CreateCommandHeader((byte)commandType, messageSize));
            packetBytes.AddRange(localAddress.ToByteArray());
            packetBytes.AddRange(destAddress.ToByteArray());
            packetBytes.AddRange(payload);
            // Return created packet
            return packetBytes.ToArray();
        }

        internal static byte[][] CreateMultipartPackets(NodeAddress destAddress, NodeAddress localAddress, byte[] multipartPayload)
        {
            byte commandType = (byte)SerialCommandType.MultipartPacket;
            // Determine how many packets we are going to create
            int totalNumPackets = multipartPayload.Length / MAX_PAYLOAD_BYTES;
            if (multipartPayload.Length % MAX_PAYLOAD_BYTES != 0)
            {
                totalNumPackets++;
            }
            List<byte> currPacketBytes = new List<byte>();
            int payloadLeft = multipartPayload.Length;
            int payloadInd = 0;
            int currPayloadLength = 0;
            byte[][] allPacketParts = new byte[totalNumPackets][];
            for (int i = 0; i < totalNumPackets; i++)
            {
                if (payloadLeft >= MAX_PAYLOAD_BYTES)
                {
                    currPayloadLength = MAX_PAYLOAD_BYTES;
                }
                else
                {
                    currPayloadLength = payloadLeft;
                }

                // Add in the command header
                byte[] serialHeader = PacketHelper.CreateCommandHeader(commandType, (byte)(currPayloadLength + 12));
                currPacketBytes.AddRange(serialHeader);
                byte[] packetHeader = PacketHelper.CreateCommandHeader(commandType, (byte)(currPayloadLength + 10));
                currPacketBytes.AddRange(packetHeader);

                // Add in address information
                currPacketBytes.AddRange(localAddress.ToByteArray());
                currPacketBytes.AddRange(destAddress.ToByteArray());

                // Add in multipacket count information
                currPacketBytes.Add(123); // default msg id for now
                currPacketBytes.Add(0); // reserved
                currPacketBytes.Add((byte)(i + 1));
                currPacketBytes.Add((byte)totalNumPackets);

                // Add in payload now
                byte[] currPayload = new byte[currPayloadLength];
                Array.Copy(multipartPayload, payloadInd, currPayload, 0, currPayloadLength);
                payloadInd += currPayloadLength;
                payloadLeft -= currPayloadLength;
                currPacketBytes.AddRange(currPayload);

                // Send out the packet now
                allPacketParts[i] = currPacketBytes.ToArray();
                currPacketBytes.Clear();
            }
            return allPacketParts;
        }

        internal static byte[] CreateCommandHeader(byte commandType, byte payloadSize)
        {
            return new byte[]
            {
                commandType, payloadSize
            };
        }
    }
}
