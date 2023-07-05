using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private void RequestDogeCoinAddress(NodeAddress destNode)
        {
            // Create payload
            byte[] payload = new byte[] { (byte)DogeCommandType.GetDogeAddress };
            portManager.SendPacket(localAddress, destNode, payload);
        }

        private void RequestDogeCoinBalance(NodeAddress destNode, byte[] dogeCoinAddress)
        {
            List<byte> payload = new List<byte>(dogeCoinAddress.Length + 1);
            payload.Add((byte)DogeCommandType.RequestBalance);
            payload.AddRange(dogeCoinAddress);
            portManager.SendPacket(localAddress, destNode, payload.ToArray());
        }

        private void SendDogeCoinBalance(NodeAddress destNode, byte[] serializedBalance)
        {
            List<byte> payload = new List<byte>(serializedBalance.Length + 1);
            payload.Add((byte)DogeCommandType.BalanceReceived);
            payload.AddRange(serializedBalance);
            portManager.SendPacket(localAddress, destNode, payload.ToArray());
        }

        private void SendGenericDogeResponse(NodeAddress destNode, bool success, DogeCommandType responseCommand, DogeResponseCode responseCode)
        {
            List<byte> payload = new List<byte>(3);
            DogeCommandType header = success ? DogeCommandType.DogeCommandSuccess : DogeCommandType.DogeCommandFailure;
            payload.Add((byte)header);
            payload.Add((byte)responseCommand);
            payload.Add((byte)responseCode);
            portManager.SendPacket(localAddress, destNode, payload.ToArray());
        }

        private void SendTransactionResult(NodeAddress destNode, string txid)
        {
            byte[] serializedTXID = Encoding.ASCII.GetBytes(txid);
            List<byte> payload = new List<byte>(serializedTXID.Length + 1);
            payload.Add((byte)DogeCommandType.TransactionResult);
            payload.AddRange(serializedTXID);
            portManager.SendPacket(localAddress, destNode, payload.ToArray());
        }

        /// <summary>
        /// Currently just generates a test address and sends it
        /// </summary>
        /// <param name="destNode"></param>
        private void SendDogeAddress(NodeAddress destNode)
        {
            // Generate a test address for now
            string privatekey;
            string publickey;

            int len = 256;
            StringBuilder pvkey = new StringBuilder(len);
            StringBuilder pubkey = new StringBuilder(len);
            LibDogecoin.dogecoin_ecc_start();
            int successret = LibDogecoin.generatePrivPubKeypair(pvkey, pubkey, 0);

            if (successret == 1)
            {
                privatekey = pvkey.ToString();
                publickey = pubkey.ToString();
                Console.WriteLine($"Public key: {publickey}");

                byte[] dogeCoinAddress = Encoding.ASCII.GetBytes(publickey);
                List<byte> payload = new List<byte>(dogeCoinAddress.Length + 1);
                payload.Add((byte)DogeCommandType.SendDogeAddress);
                payload.AddRange(dogeCoinAddress);
                portManager.SendPacket(localAddress, destNode, payload.ToArray());
            }

            LibDogecoin.dogecoin_ecc_stop();
        }
    }
}
