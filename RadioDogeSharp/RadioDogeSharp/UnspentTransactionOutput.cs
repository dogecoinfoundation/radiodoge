using System;
using System.Text;

namespace RadioDoge
{
    public class UnspentTransactionOutput
    {
        private const int VOUT_LENGTH = 4;
        private const int ID_LENGTH = 64;
        private const int AMOUNT_LENGTH = 8;
        private readonly int vout;
        private readonly string txId;
        private readonly UInt64 amount; 

        public UnspentTransactionOutput(int vout, string txId, UInt64 amount)
        {
            this.vout = vout;
            this.txId = txId;
            this.amount = amount;
        }

        public int GetVout()
        {
            return vout;
        }

        public UInt64 GetAmount()
        {
            return amount;
        }

        public string GetTxId()
        {
            return txId;
        }

        public byte[] Serialize()
        {
            List<byte> bytes = new List<byte>();
            byte[] voutBytes = BitConverter.GetBytes(vout);
            bytes.AddRange(voutBytes);
            byte[] idBytes = Encoding.ASCII.GetBytes(GetTxId());
            bytes.AddRange(idBytes);
            byte[] amountBytes = BitConverter.GetBytes(amount);
            bytes.AddRange(amountBytes);
            return bytes.ToArray();
        }
    }
}
