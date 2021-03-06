-- Calcium Object Model description for module "erm"

Type "erm.Entity"
{
	entity = "erm.IEntity"
}

Type "erm.Model"
{
	model = "erm.IModel"
}

Type "erm.Relationship"
{
	relationship = "erm.IRelationship",
	entityA = "erm.IEntity",
}

Type "erm.Multiplicity"
{
	min = "int32",
	max = "int32",
}

Type "erm.IEntity"
{
	name = "string",
	parent = "erm.IEntity",
}

Type "erm.IModel"
{
	entities = "erm.IEntity[]",
	throwsOnGet = "bool" ,
	relationships = "erm.IRelationship[]",
}

Type "erm.IRelationship"
{
	entityA = "erm.IEntity",
	entityB = "erm.IEntity",
	multiplicityA = "erm.Multiplicity",
	multiplicityB = "erm.Multiplicity",
}
