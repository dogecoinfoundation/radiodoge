namespace RadioDoge
{
    public enum PacketReconstructionCode
    {
        Incomplete,
        Complete,
        MissingPiece
    }

    class MultipartPacket
    {
        public NodeAddress senderAddress;
        public NodeAddress destinationAddress;
        public byte messageID;
        public byte reservedValue;
        private byte currentPart;
        public byte totalNumberParts;
        private List<byte> reassembledPayload;

        public MultipartPacket()
        {
            reassembledPayload = new List<byte>();
            currentPart = 1;
        }

        public PacketReconstructionCode AddPacketPiece(byte[] rawPayloadPiece)
        {
            // Verify that we didn't miss any pieces
            if (currentPart == rawPayloadPiece[8])
            {
                // If it is the first piece we will extract address information other info
                if (currentPart == 1)
                {
                    // Extract the address information
                    senderAddress = new NodeAddress(rawPayloadPiece[0], rawPayloadPiece[1], rawPayloadPiece[2]);
                    destinationAddress = new NodeAddress(rawPayloadPiece[3], rawPayloadPiece[4], rawPayloadPiece[5]);
                    // Extract msg Id and reserved value
                    messageID = rawPayloadPiece[6];
                    reservedValue = rawPayloadPiece[7];
                    // 8th byte here will be the count / current piece number
                    totalNumberParts = rawPayloadPiece[9];
                }

                // Now extract the payload
                byte[] dataPortion = new byte[rawPayloadPiece.Length - 10];
                Array.Copy(rawPayloadPiece, 10, dataPortion, 0, dataPortion.Length);
                reassembledPayload.AddRange(dataPortion);
            }
            else
            {
                return PacketReconstructionCode.MissingPiece;
            }

            // If we have gotten all the pieces then we can return 1 indicating reconstruction was successful
            if (currentPart == totalNumberParts)
            {
                return PacketReconstructionCode.Complete;
            }
            currentPart++;
            return PacketReconstructionCode.Incomplete;
        }

        public byte[] GetPayload()
        {
            return reassembledPayload.ToArray();
        }

        public override string ToString()
        {
            return "Multipart Packet\n" + $"Sender: {senderAddress}\n" + $"Message ID: {messageID}\n" + $"Total Number Parts: {totalNumberParts}\n";
        }
    }
}
