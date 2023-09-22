namespace RadioDoge
{
    class NodeAddress
    {
        public byte region, community, node;

        public NodeAddress(byte region, byte community, byte node)
        {
            this.region = region;
            this.community = community;
            this.node = node;
        }

        public override string ToString()
        {
            return $"{region}.{community}.{node}";
        }

        public byte[] ToByteArray()
        {
            return new byte[] { region, community, node };
        }
    }
}
