using System;

namespace RadioDoge
{
    public class UnspentTransactionOutput
    {
        private const int SERIALIZED_LENGTH = 35;
        private const int ID_LENGTH = 32;
        private readonly byte index;
        private readonly byte[] txId;
        private readonly uint amount; // should only be 16 bits

        public UnspentTransactionOutput(byte[] serializedUTXO)
        {
            if (serializedUTXO.Length != SERIALIZED_LENGTH)
            {
                throw new ArgumentException("Invalid number of serialized bytes supplied!");
            }

            index = serializedUTXO[0];
            amount = (uint)serializedUTXO[1] << 8;
            amount |= (uint)serializedUTXO[2];
            txId = new byte[ID_LENGTH];
            Array.Copy(serializedUTXO, 3, txId, 0, ID_LENGTH);
        }

        public byte GetIndex()
        {
            return index;
        }

        public uint GetAmount()
        {
            return amount;
        }

        public byte[] Serialize()
        {
            byte[] serialized = new byte[SERIALIZED_LENGTH];
            serialized[0] = index;
            serialized[1] = (byte)((amount >> 8) & 0xff);
            serialized[2] = (byte)(amount & 0xff);
            Array.Copy(txId, 0, serialized, 3, txId.Length);
            return serialized;
        }
    }
}
