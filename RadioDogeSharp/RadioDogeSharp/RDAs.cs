namespace RadioDoge
{
    internal static class RDAs
    {
        readonly static byte[] attn = { 0x53, 0x4F }; //SO - "AT" equivalent (cmd attention)
        readonly static byte[] result = { 0x4D, 0x55, 0x43, 0x48 }; //MUCH - Result code follows.
        readonly static byte[] ack = { 0x41, 0x43, 0x4B }; //ACK - acknowledge
        readonly static byte[] nak = { 0x4E, 0x41, 0x4B }; //NAK - remote error or nack
        readonly static byte[] err = { 0x57, 0x41, 0x54 }; //WAT - local error
        readonly static byte[] rdaFrom = { 0x46, 0x52 }; // FR (from) Node Address follows[3]
        readonly static byte[] radTo = { 0x54, 0x4F }; // TO noun is Node Address follows[3]
        readonly static byte[] rxal = { 0x52, 0x58 }; // RX - received packet alert
        readonly static byte[] rdas_this = { 0x4D, 0x49, 0x4E, 0x45 };// MINE: (message alert is for me) MSGTYPE, FR, ADDR
        readonly static byte[] msg = { 0x4D, 0x53, 0x47 };//MSG, noun follows is a clear msg
        readonly static byte[] quer = { 0x47, 0x49, 0x42 };//GIB - verb is a remote query
        readonly static byte[] ansr = { 0x57, 0x4F, 0x57 };//WOW - this is a response from a query
        readonly static byte[] txn = { 0x54, 0x58, 0x4E };//TXN - noun is an outgoing transaction
        readonly static byte[] bal = { 0x42, 0x41, 0x4C };//BAL - noun is balance (or returnval is balance)
        readonly static byte[] amtd = { 0x41, 0x4D, 0x54 };//AMT - amount in whole dogecoin
        readonly static byte[] koin = { 0x4B, 0x4E, 0x55 };//KNU - Mantissa amount in koinu
        readonly static byte[] dest = { 0x44, 0x53, 0x54 };//DST - Noun that follows is a destination (dogecoin address)
        readonly static byte[] src = { 0x53, 0x52, 0x43 };//SRC - Noun that follows is a source (utxo, index, address)
        readonly static byte[] pay = { 0x50, 0x41, 0x59 };//PAY - ask the node to pay AMOUNT/KOINU
        readonly static byte[] ask = { 0x41, 0x53, 0x4B };//ASK - request the node for a payment of AMOUNT/KOINU
        readonly static byte[] addr = { 0x41, 0x44, 0x52 };//ADR - noun that follows is a dogecoin address
        readonly static byte[] utxo = { 0x55, 0x54, 0x58 };//UTX - noun that follows is a UTXO
        readonly static byte[] indx = { 0x49, 0x44, 0x58 };//IDX - noun that follows is as utxo index.
        readonly static byte[] txid = { 0x54, 0x49, 0x44 };//TID - noun that follows is a transaction id.
        readonly static byte[] blk = { 0x42, 0x4C, 0x4B };//BLK - noun that follows is a block/height.
        readonly static byte[] qr = { 0x51, 0x52, 0x43 };//QRC - noun that follows is a QR Code 1-bit bmp data stream
    }
}
