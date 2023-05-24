using System;

namespace RadioDoge
{
    public class UnspentTransactionOutput
    {
        private const int SERIALIZED_LENGTH = 41;
        private const int ID_LENGTH = 32;
        private const int AMOUNT_LENGTH = 8;
        private readonly byte index;
        private readonly byte[] txId;
        private readonly UInt64 amount; 

        public UnspentTransactionOutput(byte[] serializedUTXO)
        {
            if (serializedUTXO.Length != SERIALIZED_LENGTH)
            {
                throw new ArgumentException("Invalid number of serialized bytes supplied!");
            }

            index = serializedUTXO[0];
            txId = new byte[ID_LENGTH];
            Array.Copy(serializedUTXO, 1, txId, 0, ID_LENGTH);
            byte[] serializedAmount = new byte[AMOUNT_LENGTH];
            Array.Copy(serializedUTXO, 33, serializedAmount, 0, AMOUNT_LENGTH);
            amount = BitConverter.ToUInt64(serializedAmount);
        }

        public byte GetIndex()
        {
            return index;
        }

        public UInt64 GetAmount()
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
