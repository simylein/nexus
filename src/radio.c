const char *radio_table = "radio";
const char *radio_schema = "create table radio ("
													 "id blob primary key, "
													 "device text not null unique, "
													 "frequency integer not null, "
													 "tx_power integer not null, "
													 "conding_rate integer not null, "
													 "bandwidth integer not null, "
													 "spreading_factor integer not null, "
													 "checksum integer not null, "
													 "sync_word integer not null"
													 ")";
